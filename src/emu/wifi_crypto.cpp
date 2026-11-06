// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "wifi_crypto.hpp"

#include "sha.hpp"

#include <algorithm>

namespace simnodus::emu {

namespace {

constexpr std::array<std::uint8_t, 256> sbox = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

std::uint8_t xtime(std::uint8_t v)
{
    return static_cast<std::uint8_t>((v << 1) ^ ((v & 0x80u) ? 0x1Bu : 0u));
}

} // namespace

std::array<std::uint8_t, 20> sha1(const Bytes& data)
{
    ShaState state = sha1_initial();
    Bytes padded = data;
    padded.push_back(0x80);
    while (padded.size() % 64 != 56) {
        padded.push_back(0);
    }
    const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8;
    for (int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
    }
    for (std::size_t i = 0; i < padded.size(); i += 64) {
        sha1_compress(state, padded.data() + i);
    }
    std::array<std::uint8_t, 20> out{};
    for (int w = 0; w < 5; ++w) {
        for (int b = 0; b < 4; ++b) {
            out[static_cast<std::size_t>(4 * w + b)] = static_cast<std::uint8_t>(state[static_cast<std::size_t>(w)] >> (24 - 8 * b));
        }
    }
    return out;
}

std::array<std::uint8_t, 20> hmac_sha1(const Bytes& key, const Bytes& data)
{
    Bytes k = key;
    if (k.size() > 64) {
        const auto h = sha1(k);
        k.assign(h.begin(), h.end());
    }
    k.resize(64, 0);
    Bytes inner(64), outer(64);
    for (std::size_t i = 0; i < 64; ++i) {
        inner[i] = static_cast<std::uint8_t>(k[i] ^ 0x36u);
        outer[i] = static_cast<std::uint8_t>(k[i] ^ 0x5Cu);
    }
    inner.insert(inner.end(), data.begin(), data.end());
    const auto ih = sha1(inner);
    outer.insert(outer.end(), ih.begin(), ih.end());
    return sha1(outer);
}

Bytes pbkdf2_sha1(const std::string& passphrase, const Bytes& salt, unsigned iterations, std::size_t length)
{
    const Bytes key(passphrase.begin(), passphrase.end());
    Bytes out;
    for (std::uint32_t block = 1; out.size() < length; ++block) {
        Bytes s = salt;
        for (int i = 3; i >= 0; --i) {
            s.push_back(static_cast<std::uint8_t>(block >> (8 * i)));
        }
        auto u = hmac_sha1(key, s);
        auto t = u;
        for (unsigned n = 1; n < iterations; ++n) {
            u = hmac_sha1(key, Bytes(u.begin(), u.end()));
            for (std::size_t i = 0; i < t.size(); ++i) {
                t[i] ^= u[i];
            }
        }
        out.insert(out.end(), t.begin(), t.end());
    }
    out.resize(length);
    return out;
}

Bytes prf_sha1(const Bytes& key, const std::string& label, const Bytes& data, std::size_t bytes)
{
    Bytes out;
    for (std::uint8_t i = 0; out.size() < bytes; ++i) {
        Bytes input(label.begin(), label.end());
        input.push_back(0);
        input.insert(input.end(), data.begin(), data.end());
        input.push_back(i);
        const auto h = hmac_sha1(key, input);
        out.insert(out.end(), h.begin(), h.end());
    }
    out.resize(bytes);
    return out;
}

namespace {

std::uint8_t gf_multiply(std::uint8_t a, std::uint8_t b)
{
    std::uint8_t product = 0;
    while (b) {
        if (b & 1u) {
            product ^= a;
        }
        a = xtime(a);
        b >>= 1;
    }
    return product;
}

std::array<std::uint8_t, 256> inverse_sbox()
{
    std::array<std::uint8_t, 256> inverse{};
    for (std::size_t i = 0; i < 256; ++i) {
        inverse[sbox[i]] = static_cast<std::uint8_t>(i);
    }
    return inverse;
}

} // namespace

Aes::Aes(const std::uint8_t* key, std::size_t key_bytes)
{
    const std::size_t nk = key_bytes == 32 ? 8 : 4;
    rounds_ = nk == 8 ? 14 : 10;
    const std::size_t words = 4 * static_cast<std::size_t>(rounds_ + 1);
    std::copy(key, key + 4 * nk, round_keys_.begin());
    std::uint8_t rcon = 1;
    for (std::size_t i = nk; i < words; ++i) {
        std::uint8_t t[4] = {round_keys_[4 * i - 4], round_keys_[4 * i - 3], round_keys_[4 * i - 2], round_keys_[4 * i - 1]};
        if (i % nk == 0) {
            const std::uint8_t first = t[0];
            t[0] = static_cast<std::uint8_t>(sbox[t[1]] ^ rcon);
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[first];
            rcon = xtime(rcon);
        } else if (nk == 8 && i % nk == 4) {
            for (auto& v : t) {
                v = sbox[v];
            }
        }
        for (std::size_t j = 0; j < 4; ++j) {
            round_keys_[4 * i + j] = static_cast<std::uint8_t>(round_keys_[4 * (i - nk) + j] ^ t[j]);
        }
    }
}

std::array<std::uint8_t, 16> Aes::encrypt(const std::uint8_t* block) const
{
    std::array<std::uint8_t, 16> s{};
    for (std::size_t i = 0; i < 16; ++i) {
        s[i] = static_cast<std::uint8_t>(block[i] ^ round_keys_[i]);
    }
    for (int round = 1; round <= rounds_; ++round) {
        for (auto& b : s) {
            b = sbox[b];
        }
        // ShiftRows (column-major state: byte r + 4c).
        std::array<std::uint8_t, 16> t = s;
        for (std::size_t r = 1; r < 4; ++r) {
            for (std::size_t c = 0; c < 4; ++c) {
                s[r + 4 * c] = t[r + 4 * ((c + r) % 4)];
            }
        }
        if (round != rounds_) {
            for (std::size_t c = 0; c < 4; ++c) {
                std::uint8_t* col = &s[4 * c];
                const std::uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                const std::uint8_t all = static_cast<std::uint8_t>(a0 ^ a1 ^ a2 ^ a3);
                col[0] = static_cast<std::uint8_t>(a0 ^ all ^ xtime(static_cast<std::uint8_t>(a0 ^ a1)));
                col[1] = static_cast<std::uint8_t>(a1 ^ all ^ xtime(static_cast<std::uint8_t>(a1 ^ a2)));
                col[2] = static_cast<std::uint8_t>(a2 ^ all ^ xtime(static_cast<std::uint8_t>(a2 ^ a3)));
                col[3] = static_cast<std::uint8_t>(a3 ^ all ^ xtime(static_cast<std::uint8_t>(a3 ^ a0)));
            }
        }
        for (std::size_t i = 0; i < 16; ++i) {
            s[i] ^= round_keys_[static_cast<std::size_t>(16 * round) + i];
        }
    }
    return s;
}

std::array<std::uint8_t, 16> Aes::decrypt(const std::uint8_t* block) const
{
    static const std::array<std::uint8_t, 256> inverse = inverse_sbox();
    std::array<std::uint8_t, 16> s{};
    for (std::size_t i = 0; i < 16; ++i) {
        s[i] = static_cast<std::uint8_t>(block[i] ^ round_keys_[static_cast<std::size_t>(16 * rounds_) + i]);
    }
    for (int round = rounds_ - 1; round >= 0; --round) {
        // InvShiftRows, InvSubBytes, AddRoundKey, then InvMixColumns.
        std::array<std::uint8_t, 16> t = s;
        for (std::size_t r = 1; r < 4; ++r) {
            for (std::size_t c = 0; c < 4; ++c) {
                s[r + 4 * ((c + r) % 4)] = t[r + 4 * c];
            }
        }
        for (auto& b : s) {
            b = inverse[b];
        }
        for (std::size_t i = 0; i < 16; ++i) {
            s[i] ^= round_keys_[static_cast<std::size_t>(16 * round) + i];
        }
        if (round != 0) {
            for (std::size_t c = 0; c < 4; ++c) {
                std::uint8_t* col = &s[4 * c];
                const std::uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                col[0] = static_cast<std::uint8_t>(gf_multiply(a0, 14) ^ gf_multiply(a1, 11) ^ gf_multiply(a2, 13) ^ gf_multiply(a3, 9));
                col[1] = static_cast<std::uint8_t>(gf_multiply(a0, 9) ^ gf_multiply(a1, 14) ^ gf_multiply(a2, 11) ^ gf_multiply(a3, 13));
                col[2] = static_cast<std::uint8_t>(gf_multiply(a0, 13) ^ gf_multiply(a1, 9) ^ gf_multiply(a2, 14) ^ gf_multiply(a3, 11));
                col[3] = static_cast<std::uint8_t>(gf_multiply(a0, 11) ^ gf_multiply(a1, 13) ^ gf_multiply(a2, 9) ^ gf_multiply(a3, 14));
            }
        }
    }
    return s;
}

Bytes aes_key_wrap(const std::uint8_t* kek, const Bytes& plain)
{
    const Aes128 aes(kek);
    const std::size_t n = plain.size() / 8;
    std::array<std::uint8_t, 8> a;
    a.fill(0xA6);
    Bytes r = plain;
    for (std::uint64_t j = 0; j < 6; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            std::uint8_t block[16];
            std::copy(a.begin(), a.end(), block);
            std::copy(r.begin() + static_cast<std::ptrdiff_t>(8 * i), r.begin() + static_cast<std::ptrdiff_t>(8 * i + 8), block + 8);
            const auto b = aes.encrypt(block);
            const std::uint64_t t = n * j + i + 1;
            for (int k = 0; k < 8; ++k) {
                a[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(b[static_cast<std::size_t>(k)] ^ (t >> (8 * (7 - k))));
            }
            std::copy(b.begin() + 8, b.end(), r.begin() + static_cast<std::ptrdiff_t>(8 * i));
        }
    }
    Bytes out(a.begin(), a.end());
    out.insert(out.end(), r.begin(), r.end());
    return out;
}

namespace {

// CCM's CBC-MAC (8 bytes) over B0, the encoded AAD and the message.
std::array<std::uint8_t, 8> ccm_mac(const Aes128& aes, const std::uint8_t* nonce, const Bytes& aad, const Bytes& plain)
{
    std::uint8_t x[16] = {};
    const auto absorb = [&](const std::uint8_t* block) {
        for (int i = 0; i < 16; ++i) {
            x[i] ^= block[i];
        }
        const auto e = aes.encrypt(x);
        std::copy(e.begin(), e.end(), x);
    };
    std::uint8_t b0[16];
    b0[0] = static_cast<std::uint8_t>((aad.empty() ? 0 : 0x40) | (((8 - 2) / 2) << 3) | (2 - 1));
    std::copy(nonce, nonce + 13, b0 + 1);
    b0[14] = static_cast<std::uint8_t>(plain.size() >> 8);
    b0[15] = static_cast<std::uint8_t>(plain.size());
    absorb(b0);
    if (!aad.empty()) {
        Bytes a{static_cast<std::uint8_t>(aad.size() >> 8), static_cast<std::uint8_t>(aad.size())};
        a.insert(a.end(), aad.begin(), aad.end());
        a.resize((a.size() + 15) / 16 * 16, 0);
        for (std::size_t i = 0; i < a.size(); i += 16) {
            absorb(a.data() + i);
        }
    }
    for (std::size_t i = 0; i < plain.size(); i += 16) {
        std::uint8_t block[16] = {};
        std::copy(plain.begin() + static_cast<std::ptrdiff_t>(i), plain.begin() + static_cast<std::ptrdiff_t>(std::min(plain.size(), i + 16)), block);
        absorb(block);
    }
    std::array<std::uint8_t, 8> t{};
    std::copy(x, x + 8, t.begin());
    return t;
}

// The CTR key stream block A_i.
std::array<std::uint8_t, 16> ccm_stream(const Aes128& aes, const std::uint8_t* nonce, std::uint16_t counter)
{
    std::uint8_t a[16];
    a[0] = 2 - 1;
    std::copy(nonce, nonce + 13, a + 1);
    a[14] = static_cast<std::uint8_t>(counter >> 8);
    a[15] = static_cast<std::uint8_t>(counter);
    return aes.encrypt(a);
}

void ccm_crypt(const Aes128& aes, const std::uint8_t* nonce, Bytes& data)
{
    for (std::size_t i = 0; i < data.size(); i += 16) {
        const auto s = ccm_stream(aes, nonce, static_cast<std::uint16_t>(i / 16 + 1));
        for (std::size_t k = 0; k < 16 && i + k < data.size(); ++k) {
            data[i + k] ^= s[k];
        }
    }
}

} // namespace

Bytes ccm_encrypt(const std::uint8_t* key, const std::uint8_t* nonce, const Bytes& aad, const Bytes& plain)
{
    const Aes128 aes(key);
    const auto t = ccm_mac(aes, nonce, aad, plain);
    Bytes out = plain;
    ccm_crypt(aes, nonce, out);
    const auto s0 = ccm_stream(aes, nonce, 0);
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>(t[i] ^ s0[i]));
    }
    return out;
}

bool ccm_decrypt(const std::uint8_t* key, const std::uint8_t* nonce, const Bytes& aad, const Bytes& sealed, Bytes& plain)
{
    if (sealed.size() < 8) {
        return false;
    }
    const Aes128 aes(key);
    plain.assign(sealed.begin(), sealed.end() - 8);
    ccm_crypt(aes, nonce, plain);
    const auto t = ccm_mac(aes, nonce, aad, plain);
    const auto s0 = ccm_stream(aes, nonce, 0);
    for (std::size_t i = 0; i < 8; ++i) {
        if (static_cast<std::uint8_t>(t[i] ^ s0[i]) != sealed[sealed.size() - 8 + i]) {
            return false;
        }
    }
    return true;
}

std::size_t wifi_header_length(const Bytes& frame)
{
    std::size_t length = 24;
    if (frame.size() >= 2 && (frame[1] & 0x03u) == 0x03u) {
        length += 6; // four addresses
    }
    if (!frame.empty() && (frame[0] & 0x0Cu) == 0x08u && (frame[0] & 0x80u)) {
        length += 2; // QoS control
    }
    return length;
}

namespace {

// IEEE 802.11 12.5.3.3.3-4: the AAD and the nonce of a CCMP frame.
void ccmp_aad_nonce(const Bytes& frame, std::uint64_t pn, Bytes& aad, std::uint8_t* nonce)
{
    const bool qos = (frame[0] & 0x0Cu) == 0x08u && (frame[0] & 0x80u);
    aad.clear();
    aad.push_back(static_cast<std::uint8_t>((frame[0] & 0x0Cu) == 0x08u ? frame[0] & 0x8Fu : frame[0]));
    aad.push_back(static_cast<std::uint8_t>((frame[1] & (qos ? 0x47u : 0xC7u)) | 0x40u));
    aad.insert(aad.end(), frame.begin() + 4, frame.begin() + 22); // A1, A2, A3
    aad.push_back(static_cast<std::uint8_t>(frame[22] & 0x0Fu));   // fragment number only
    aad.push_back(0);
    std::size_t at = 24;
    if ((frame[1] & 0x03u) == 0x03u) {
        aad.insert(aad.end(), frame.begin() + 24, frame.begin() + 30); // A4
        at = 30;
    }
    std::uint8_t priority = 0;
    if (qos) {
        priority = static_cast<std::uint8_t>(frame[at] & 0x0Fu);
        aad.push_back(priority);
        aad.push_back(0);
    }
    nonce[0] = priority;
    std::copy(frame.begin() + 10, frame.begin() + 16, nonce + 1); // A2
    for (int i = 0; i < 6; ++i) {
        nonce[7 + i] = static_cast<std::uint8_t>(pn >> (8 * (5 - i)));
    }
}

} // namespace

Bytes ccmp_encrypt(const std::uint8_t* tk, const Bytes& frame, std::uint64_t pn, int key_id)
{
    const std::size_t header = wifi_header_length(frame);
    Bytes aad;
    std::uint8_t nonce[13];
    ccmp_aad_nonce(frame, pn, aad, nonce);
    const Bytes body(frame.begin() + static_cast<std::ptrdiff_t>(header), frame.end());
    const Bytes sealed = ccm_encrypt(tk, nonce, aad, body);
    Bytes out(frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(header));
    out[1] |= 0x40; // Protected
    out.push_back(static_cast<std::uint8_t>(pn));
    out.push_back(static_cast<std::uint8_t>(pn >> 8));
    out.push_back(0);
    out.push_back(static_cast<std::uint8_t>(0x20 | (key_id << 6))); // ExtIV
    for (int i = 2; i < 6; ++i) {
        out.push_back(static_cast<std::uint8_t>(pn >> (8 * i)));
    }
    out.insert(out.end(), sealed.begin(), sealed.end());
    return out;
}

bool ccmp_decrypt(const std::uint8_t* tk, const Bytes& frame, Bytes& plain, std::uint64_t& pn)
{
    const std::size_t header = wifi_header_length(frame);
    if (frame.size() < header + 16) {
        return false;
    }
    const std::uint8_t* h = frame.data() + header;
    pn = static_cast<std::uint64_t>(h[0]) | (static_cast<std::uint64_t>(h[1]) << 8) | (static_cast<std::uint64_t>(h[4]) << 16)
        | (static_cast<std::uint64_t>(h[5]) << 24) | (static_cast<std::uint64_t>(h[6]) << 32) | (static_cast<std::uint64_t>(h[7]) << 40);
    Bytes aad;
    std::uint8_t nonce[13];
    ccmp_aad_nonce(frame, pn, aad, nonce);
    const Bytes sealed(frame.begin() + static_cast<std::ptrdiff_t>(header + 8), frame.end());
    Bytes body;
    if (!ccm_decrypt(tk, nonce, aad, sealed, body)) {
        return false;
    }
    plain.assign(frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(header));
    plain[1] &= static_cast<std::uint8_t>(~0x40u);
    plain.insert(plain.end(), body.begin(), body.end());
    return true;
}

} // namespace simnodus::emu
