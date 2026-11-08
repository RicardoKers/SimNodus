// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "wifi_gateway.hpp"

#include "host_socket.hpp"

extern "C" {
#include "lwip/init.h"
#include "lwip/ip4.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
}

#include <algorithm>
#include <deque>
#include <map>
#include <cctype>
#include <utility>

namespace {

std::uint32_t virtual_ms = 0; // lwIP's clock: the simulation's time

} // namespace

extern "C" u32_t sys_now(void)
{
    return virtual_ms;
}

namespace simnodus::emu {

namespace {

constexpr std::uint8_t protocol_icmp = 1;
constexpr std::uint8_t protocol_tcp = 6;
constexpr std::uint8_t protocol_udp = 17;
constexpr std::uint16_t listen_port = 1; // lwIP's listener for rewritten flows
constexpr Ipv4Address own{192, 168, 4, 1};
constexpr std::uint64_t udp_idle_ns = 120'000'000'000ull;

std::uint16_t be16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

void set16(std::uint8_t* p, std::uint16_t v)
{
    p[0] = static_cast<std::uint8_t>(v >> 8);
    p[1] = static_cast<std::uint8_t>(v);
}

Ipv4Address ip_at(const std::uint8_t* p)
{
    return {p[0], p[1], p[2], p[3]};
}

// The IPv4 header's and the TCP or UDP checksum after an address changed.
void fix_checksums(std::vector<std::uint8_t>& packet)
{
    const std::size_t header = (packet[0] & 0x0Fu) * 4u;
    const std::size_t total = std::min<std::size_t>(be16(&packet[2]), packet.size());
    set16(&packet[10], 0);
    set16(&packet[10], internet_checksum(packet.data(), header));
    const std::uint8_t protocol = packet[9];
    if ((protocol != protocol_tcp && protocol != protocol_udp) || total <= header) {
        return;
    }
    const std::size_t length = total - header;
    std::uint8_t* l4 = packet.data() + header;
    const std::size_t at = protocol == protocol_tcp ? 16 : 6;
    if (length < at + 2) {
        return;
    }
    set16(l4 + at, 0);
    std::uint32_t sum = 0;
    for (std::size_t i = 12; i < 20; i += 2) {
        sum += be16(&packet[i]);
    }
    sum += protocol + static_cast<std::uint32_t>(length);
    std::uint16_t check = internet_checksum(l4, length, sum);
    if (protocol == protocol_udp && check == 0) {
        check = 0xFFFF;
    }
    set16(l4 + at, check);
}

HostAddress outside(const Ipv4Address& address)
{
    if (address == WifiGateway::computer) {
        return {127, 0, 0, 1};
    }
    return address;
}

} // namespace

using Endpoint = std::pair<Ipv4Address, std::uint16_t>;

struct Flow {
    tcp_pcb* pcb = nullptr;
    std::unique_ptr<HostSocket> socket;
    bool host_ready = false;    // the computer's side is connected
    bool station_ready = false; // lwIP's side is established
    bool station_closed = false;
    bool host_closed = false;
    bool host_shut = false;
    bool dead = false;
    std::deque<std::uint8_t> to_host;
};

struct UdpFlow {
    std::unique_ptr<HostSocket> socket;
    std::uint64_t last_ns = 0;
};

struct WifiGateway::State {
    netif nif{};
    tcp_pcb* listener = nullptr;
    std::vector<std::unique_ptr<Flow>> flows;
    std::map<Endpoint, Endpoint> nat; // a station's endpoint -> the address it connected to
    std::map<Endpoint, UdpFlow> udp;
    std::vector<std::pair<WifiForward, std::unique_ptr<HostSocket>>> listeners;
    std::vector<std::vector<std::uint8_t>> out;
    std::uint64_t now_ns = 0;
    // Pings to the outside: the station's echo request, kept for the reply.
    struct Ping {
        Ipv4Address station{};
        Ipv4Address remote{};
        std::vector<std::uint8_t> icmp;
    };
    HostPinger pinger;
    std::map<std::uint64_t, Ping> pings;
    std::uint64_t next_ping = 1;
};

namespace {

WifiGateway::State* running = nullptr;

err_t netif_output(struct netif* nif, struct pbuf* p, const ip4_addr_t* /*to*/)
{
    auto* state = static_cast<WifiGateway::State*>(nif->state);
    std::vector<std::uint8_t> packet(p->tot_len);
    pbuf_copy_partial(p, packet.data(), p->tot_len, 0);
    if (packet.size() >= 24 && packet[9] == protocol_tcp) {
        const std::size_t header = (packet[0] & 0x0Fu) * 4u;
        // An answer on a rewritten flow: the original address comes back.
        if (packet.size() >= header + 4 && be16(&packet[header]) == listen_port) {
            const auto it = state->nat.find({ip_at(&packet[16]), be16(&packet[header + 2])});
            if (it != state->nat.end()) {
                std::copy(it->second.first.begin(), it->second.first.end(), packet.begin() + 12);
                set16(&packet[header], it->second.second);
                fix_checksums(packet);
            }
        }
    }
    state->out.push_back(std::move(packet));
    return ERR_OK;
}

err_t netif_setup(struct netif* nif)
{
    nif->output = netif_output;
    nif->mtu = 1500;
    nif->flags = NETIF_FLAG_UP | NETIF_FLAG_LINK_UP;
    return ERR_OK;
}

void flow_error(void* arg, err_t /*err*/)
{
    if (auto* flow = static_cast<Flow*>(arg)) {
        flow->pcb = nullptr; // lwIP freed it
        flow->dead = true;
    }
}

err_t flow_receive(void* arg, tcp_pcb* /*pcb*/, pbuf* p, err_t /*err*/)
{
    auto* flow = static_cast<Flow*>(arg);
    if (!flow) {
        if (p) {
            pbuf_free(p);
        }
        return ERR_OK;
    }
    if (!p) {
        flow->station_closed = true;
        return ERR_OK;
    }
    for (pbuf* q = p; q; q = q->next) {
        const auto* bytes = static_cast<const std::uint8_t*>(q->payload);
        flow->to_host.insert(flow->to_host.end(), bytes, bytes + q->len);
    }
    pbuf_free(p); // the window opens as the bytes reach the computer
    return ERR_OK;
}

void attach(Flow* flow, tcp_pcb* pcb)
{
    flow->pcb = pcb;
    tcp_arg(pcb, flow);
    tcp_recv(pcb, flow_receive);
    tcp_err(pcb, flow_error);
}

err_t flow_accept(void* arg, tcp_pcb* pcb, err_t err)
{
    auto* state = static_cast<WifiGateway::State*>(arg);
    if (err != ERR_OK || !pcb) {
        return ERR_VAL;
    }
    const Ipv4Address station{ip4_addr1(&pcb->remote_ip), ip4_addr2(&pcb->remote_ip), ip4_addr3(&pcb->remote_ip),
        ip4_addr4(&pcb->remote_ip)};
    const auto it = state->nat.find({station, pcb->remote_port});
    std::unique_ptr<HostSocket> socket;
    if (it != state->nat.end()) {
        socket = HostSocket::connect_tcp(outside(it->second.first), it->second.second);
    }
    if (!socket) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }
    auto flow = std::make_unique<Flow>();
    flow->socket = std::move(socket);
    flow->station_ready = true;
    attach(flow.get(), pcb);
    state->flows.push_back(std::move(flow));
    return ERR_OK;
}

err_t forward_connected(void* arg, tcp_pcb* /*pcb*/, err_t err)
{
    if (auto* flow = static_cast<Flow*>(arg); flow && err == ERR_OK) {
        flow->station_ready = true;
    }
    return ERR_OK;
}

} // namespace

WifiGateway::WifiGateway() : state_(std::make_unique<State>()) {}

std::unique_ptr<WifiGateway> WifiGateway::start(const std::vector<WifiForward>& forwards, std::string& error)
{
    if (running) {
        error = "another Wi-Fi gateway is running";
        return nullptr;
    }
    std::unique_ptr<WifiGateway> gateway(new WifiGateway());
    State& s = *gateway->state_;
    for (const WifiForward& forward : forwards) {
        auto listener = HostSocket::listen_tcp(forward.host_port, error);
        if (!listener) {
            return nullptr;
        }
        s.listeners.emplace_back(forward, std::move(listener));
    }
    static const bool initialized = [] {
        lwip_init();
        return true;
    }();
    (void)initialized;
    ip4_addr_t address;
    ip4_addr_t mask;
    ip4_addr_t none;
    IP4_ADDR(&address, own[0], own[1], own[2], own[3]);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&none, 0, 0, 0, 0);
    netif_add(&s.nif, &address, &mask, &none, &s, netif_setup, ip4_input);
    netif_set_default(&s.nif);
    netif_set_up(&s.nif);
    netif_set_link_up(&s.nif);
    tcp_pcb* pcb = tcp_new();
    tcp_bind(pcb, &address, listen_port);
    s.listener = tcp_listen(pcb);
    tcp_arg(s.listener, &s);
    tcp_accept(s.listener, flow_accept);
    running = &s;
    return gateway;
}

WifiGateway::~WifiGateway()
{
    State& s = *state_;
    for (auto& flow : s.flows) {
        if (flow->pcb) {
            tcp_arg(flow->pcb, nullptr);
            tcp_abort(flow->pcb);
        }
    }
    if (s.listener) {
        tcp_close(s.listener);
    }
    netif_remove(&s.nif);
    running = nullptr;
}

void WifiGateway::from_station(const std::vector<std::uint8_t>& ip_packet, std::uint64_t now_ns)
{
    State& s = *state_;
    s.now_ns = now_ns;
    virtual_ms = static_cast<std::uint32_t>(now_ns / 1'000'000);
    if (ip_packet.size() < 20) {
        return;
    }
    const std::size_t header = (ip_packet[0] & 0x0Fu) * 4u;
    const std::size_t total = std::min<std::size_t>(be16(&ip_packet[2]), ip_packet.size());
    if (total < header + 8) {
        return;
    }
    const Ipv4Address src = ip_at(&ip_packet[12]);
    const Ipv4Address dst = ip_at(&ip_packet[16]);
    const std::uint16_t src_port = be16(&ip_packet[header]);
    const std::uint16_t dst_port = be16(&ip_packet[header + 2]);
    std::vector<std::uint8_t> packet(ip_packet.begin(), ip_packet.begin() + static_cast<std::ptrdiff_t>(total));
    if (packet[9] == protocol_tcp) {
        if (dst != own) {
            const std::uint8_t flags = packet[header + 13];
            if ((flags & 0x02u) && !(flags & 0x10u)) { // SYN: a new flow
                s.nat[{src, src_port}] = {dst, dst_port};
            }
            if (!s.nat.count({src, src_port})) {
                return;
            }
            std::copy(own.begin(), own.end(), packet.begin() + 16);
            set16(&packet[header + 2], listen_port);
            fix_checksums(packet);
        }
        pbuf* p = pbuf_alloc(PBUF_RAW, static_cast<u16_t>(packet.size()), PBUF_RAM);
        if (!p) {
            return;
        }
        pbuf_take(p, packet.data(), static_cast<u16_t>(packet.size()));
        if (s.nif.input(p, &s.nif) != ERR_OK) {
            pbuf_free(p);
        }
    } else if (packet[9] == protocol_icmp) {
        // An echo request: the computer pings, the answer comes back later.
        if (packet[header] != 8 || s.pings.size() >= 64) {
            return;
        }
        const std::uint64_t tag = s.next_ping++;
        State::Ping& ping = s.pings[tag];
        ping.station = src;
        ping.remote = dst;
        ping.icmp.assign(packet.begin() + static_cast<std::ptrdiff_t>(header), packet.end());
        s.pinger.send(outside(dst), std::vector<std::uint8_t>(ping.icmp.begin() + 8, ping.icmp.end()), tag);
    } else if (packet[9] == protocol_udp) {
        UdpFlow& flow = s.udp[{src, src_port}];
        if (!flow.socket) {
            flow.socket = HostSocket::open_udp();
            if (!flow.socket) {
                s.udp.erase({src, src_port});
                return;
            }
        }
        flow.last_ns = now_ns;
        flow.socket->send_to(outside(dst), dst_port, packet.data() + header + 8, total - header - 8);
    }
}

void WifiGateway::poll(std::uint64_t now_ns)
{
    State& s = *state_;
    s.now_ns = now_ns;
    virtual_ms = static_cast<std::uint32_t>(now_ns / 1'000'000);
    sys_check_timeouts();
    // New connections to the forwarded ports.
    for (auto& [forward, listener] : s.listeners) {
        while (auto socket = listener->accept()) {
            tcp_pcb* pcb = tcp_new();
            if (!pcb) {
                break;
            }
            auto flow = std::make_unique<Flow>();
            flow->socket = std::move(socket);
            flow->host_ready = true;
            attach(flow.get(), pcb);
            ip4_addr_t to;
            IP4_ADDR(&to, forward.station[0], forward.station[1], forward.station[2], forward.station[3]);
            if (tcp_connect(pcb, &to, forward.station_port, forward_connected) != ERR_OK) {
                tcp_arg(pcb, nullptr);
                tcp_abort(pcb);
                continue;
            }
            s.flows.push_back(std::move(flow));
        }
    }
    std::uint8_t buffer[4096];
    for (auto& flow : s.flows) {
        Flow& f = *flow;
        if (f.dead) {
            continue;
        }
        if (!f.host_ready) {
            const int done = f.socket->finished();
            if (done > 0) {
                f.host_ready = true;
            } else if (done < 0) {
                if (f.pcb) {
                    tcp_arg(f.pcb, nullptr);
                    tcp_abort(f.pcb); // the station sees a reset
                    f.pcb = nullptr;
                }
                f.dead = true;
                continue;
            }
        }
        if (f.host_ready && !f.to_host.empty()) {
            const std::size_t count = std::min<std::size_t>(f.to_host.size(), sizeof(buffer));
            std::copy(f.to_host.begin(), f.to_host.begin() + static_cast<std::ptrdiff_t>(count), buffer);
            const long sent = f.socket->send(buffer, count);
            if (sent < 0) {
                f.dead = true;
            } else if (sent > 0) {
                f.to_host.erase(f.to_host.begin(), f.to_host.begin() + sent);
                if (f.pcb) {
                    tcp_recved(f.pcb, static_cast<u16_t>(sent));
                }
            }
        }
        if (f.station_closed && f.to_host.empty() && f.host_ready && !f.host_shut) {
            f.socket->shutdown_send();
            f.host_shut = true;
        }
        if (f.host_ready && f.station_ready && f.pcb && !f.host_closed) {
            const std::size_t room = std::min<std::size_t>(tcp_sndbuf(f.pcb), sizeof(buffer));
            if (room > 0) {
                const long n = f.socket->receive(buffer, room);
                if (n > 0) {
                    tcp_write(f.pcb, buffer, static_cast<u16_t>(n), TCP_WRITE_FLAG_COPY);
                    tcp_output(f.pcb);
                } else if (n == 0) {
                    f.host_closed = true;
                    tcp_shutdown(f.pcb, 0, 1); // FIN to the station
                } else if (n == -2) {
                    f.dead = true;
                }
            }
        }
        if (f.dead && f.pcb) {
            tcp_arg(f.pcb, nullptr);
            tcp_abort(f.pcb);
            f.pcb = nullptr;
        }
        if (f.host_closed && f.station_closed && f.to_host.empty() && f.pcb) {
            tcp_arg(f.pcb, nullptr);
            if (tcp_close(f.pcb) != ERR_OK) {
                tcp_abort(f.pcb);
            }
            f.pcb = nullptr;
            f.dead = true;
        }
    }
    s.flows.erase(std::remove_if(s.flows.begin(), s.flows.end(), [](const auto& f) { return f->dead; }), s.flows.end());
    // Pings that came back: an echo reply from the address the station
    // pinged; the others time out on the station, as without an answer.
    for (const HostPinger::Result& result : s.pinger.take()) {
        const auto ping = s.pings.find(result.tag);
        if (ping == s.pings.end()) {
            continue;
        }
        if (result.answered) {
            std::vector<std::uint8_t> icmp = ping->second.icmp;
            icmp[0] = 0; // echo reply
            icmp[2] = 0;
            icmp[3] = 0;
            set16(&icmp[2], internet_checksum(icmp.data(), icmp.size()));
            std::vector<std::uint8_t> reply{0x45, 0, 0, 0, 0, 0, 0, 0, 64, protocol_icmp, 0, 0};
            reply.insert(reply.end(), ping->second.remote.begin(), ping->second.remote.end());
            reply.insert(reply.end(), ping->second.station.begin(), ping->second.station.end());
            reply.insert(reply.end(), icmp.begin(), icmp.end());
            set16(&reply[2], static_cast<std::uint16_t>(reply.size()));
            fix_checksums(reply);
            s.out.push_back(std::move(reply));
        }
        s.pings.erase(ping);
    }
    // UDP answers.
    for (auto it = s.udp.begin(); it != s.udp.end();) {
        HostAddress from{};
        std::uint16_t from_port = 0;
        long n;
        while ((n = it->second.socket->receive_from(buffer, sizeof(buffer), from, from_port)) >= 0) {
            const Ipv4Address source = from == HostAddress{127, 0, 0, 1} ? computer : Ipv4Address(from);
            std::vector<std::uint8_t> packet{0x45, 0, 0, 0, 0, 0, 0, 0, 64, protocol_udp, 0, 0};
            packet.insert(packet.end(), source.begin(), source.end());
            packet.insert(packet.end(), it->first.first.begin(), it->first.first.end());
            const std::size_t total = 20 + 8 + static_cast<std::size_t>(n);
            set16(&packet[2], static_cast<std::uint16_t>(total));
            packet.resize(28);
            set16(&packet[20], from_port);
            set16(&packet[22], it->first.second);
            set16(&packet[24], static_cast<std::uint16_t>(8 + n));
            packet.insert(packet.end(), buffer, buffer + n);
            fix_checksums(packet);
            s.out.push_back(std::move(packet));
            it->second.last_ns = now_ns;
        }
        if (now_ns > it->second.last_ns + udp_idle_ns) {
            it = s.udp.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::vector<std::uint8_t>> WifiGateway::take_to_stations()
{
    std::vector<std::vector<std::uint8_t>> out;
    out.swap(state_->out);
    return out;
}

std::optional<Ipv4Address> WifiGateway::resolve(const std::string& name) const
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "host.simnodus") {
        return computer;
    }
    if (const auto address = host_resolve(name)) {
        return Ipv4Address(*address);
    }
    return std::nullopt;
}

} // namespace simnodus::emu
