// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Manual disposable-VM request probe. No production IPC or automatic CTest run.
#include "platform/windows/managed_store.hpp"
#include "application/declaration_ingress.hpp"
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace simnodus::experimental;
constexpr DWORD client_rights = 0x120183, writer_rights = 0x1f01ff;
constexpr std::size_t request_limit = 32 + 176 + simnodus::declaration_max_bytes;
constexpr std::size_t response_limit = 32 + 4 + 108 + 4 + managed_context_max_bytes + 4 + simnodus::declaration_max_bytes;
using Clock = std::chrono::steady_clock;
struct Failure { const char* code; DWORD system{}; };
void need(bool value, const char* code) { if(!value) throw Failure{code}; }
void native(BOOL value, const char* code) { if(!value) throw Failure{code, GetLastError()}; }
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE input = INVALID_HANDLE_VALUE) : value(input) {}
    ~Handle() { if(value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Local { void* value{}; ~Local() { if(value) LocalFree(value); } };
std::wstring wide(std::string_view input) { return {input.begin(), input.end()}; }
std::string hex(const auto& input)
{
    std::string output;
    for(unsigned char value : input) { output += "0123456789abcdef"[value >> 4]; output += "0123456789abcdef"[value & 15]; }
    return output;
}
std::string sid_text(PSID sid)
{
    LPWSTR text = nullptr; native(ConvertSidToStringSidW(sid, &text), "sid-text"); Local allocation{text};
    std::string result;
    for(wchar_t c : std::wstring_view(text)) { need(c > 0 && c < 128, "sid-text"); result += static_cast<char>(c); }
    return result;
}
std::string sid_bytes(std::string_view text)
{
    PSID sid = nullptr; native(ConvertStringSidToSidW(wide(text).c_str(), &sid), "sid"); Local allocation{sid};
    need(IsValidSid(sid) != FALSE && GetLengthSid(sid) <= 68, "sid");
    return {static_cast<const char*>(sid), GetLengthSid(sid)};
}
std::string token_sid(HANDLE token)
{
    alignas(TOKEN_USER) std::array<unsigned char, 1024> buffer{}; DWORD required = 0;
    native(GetTokenInformation(token, TokenUser, buffer.data(), static_cast<DWORD>(buffer.size()), &required), "token-user");
    PSID sid = reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid;
    need(IsValidSid(sid) != FALSE && GetLengthSid(sid) <= 68, "token-sid");
    return {static_cast<const char*>(sid), GetLengthSid(sid)};
}
std::string current_sid()
{
    HANDLE raw = nullptr; native(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw), "process-token"); Handle token(raw);
    return token_sid(token.value);
}
std::string text_of(const std::string& binary) { return sid_text(const_cast<char*>(binary.data())); }
std::wstring pipe_name(std::string_view leaf)
{
    need(leaf.size() == 22 && leaf.starts_with("SN021Save-")
        && std::all_of(leaf.begin() + 10, leaf.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "pipe-name");
    return L"\\\\.\\pipe\\" + wide(leaf);
}
std::string descriptor_text(const std::string& writer, const std::string& client, const std::string& extra,
    bool extra_client_rights = false)
{
    need(writer != client && (extra.empty() || (extra != writer && extra != client)), "pipe-principals");
    auto result = "O:" + text_of(writer) + "G:" + text_of(writer) + "D:P(A;;0x1f01ff;;;" + text_of(writer)
        + ")(A;;" + (extra_client_rights ? "0x12018b" : "0x120183") + ";;;" + text_of(client) + ")";
    if(!extra.empty()) result += "(A;;0x120183;;;" + text_of(extra) + ")";
    return result;
}
void verify_pipe(HANDLE pipe, const std::string& writer, const std::string& client, const std::string& extra)
{
    PSID owner = nullptr; PACL dacl = nullptr; PSECURITY_DESCRIPTOR raw = nullptr;
    const DWORD error = GetSecurityInfo(pipe, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, nullptr, &dacl, nullptr, &raw);
    if(error != ERROR_SUCCESS) throw Failure{"pipe-descriptor", error}; Local allocation{raw};
    need(owner && EqualSid(owner, const_cast<char*>(writer.data())) != FALSE, "pipe-owner");
    SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
    native(GetSecurityDescriptorControl(raw, &control, &revision), "pipe-control");
    need((control & SE_DACL_PROTECTED) != 0 && dacl != nullptr, "pipe-dacl");
    std::vector<std::pair<std::string, DWORD>> expected{{writer, writer_rights}, {client, client_rights}};
    if(!extra.empty()) expected.emplace_back(extra, client_rights);
    need(dacl->AceCount == expected.size(), "pipe-ace-count");
    for(DWORD i = 0; i < dacl->AceCount; ++i) {
        void* raw_ace = nullptr; native(GetAce(dacl, i, &raw_ace), "pipe-ace");
        const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw_ace);
        need(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && ace->Header.AceFlags == 0, "pipe-ace-kind");
        const auto found = std::find_if(expected.begin(), expected.end(), [&](const auto& item) {
            return EqualSid(const_cast<DWORD*>(&ace->SidStart), const_cast<char*>(item.first.data())) != FALSE && ace->Mask == item.second;
        });
        need(found != expected.end(), "pipe-rights"); expected.erase(found);
    }
    need(expected.empty(), "pipe-rights");
}
std::string observed_pipe_descriptor(HANDLE pipe)
{
    PSID owner = nullptr; PACL dacl = nullptr; PSECURITY_DESCRIPTOR raw = nullptr;
    const DWORD error = GetSecurityInfo(pipe, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, nullptr, &dacl, nullptr, &raw);
    if(error != ERROR_SUCCESS) throw Failure{"listener-descriptor", error}; Local allocation{raw};
    need(owner != nullptr && dacl != nullptr, "listener-descriptor");
    SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
    native(GetSecurityDescriptorControl(raw, &control, &revision), "listener-control");
    std::string result = "{\"owner\":\"" + sid_text(owner) + "\",\"dacl_protected\":"
        + ((control & SE_DACL_PROTECTED) ? "true" : "false") + ",\"aces\":[";
    for(DWORD i = 0; i < dacl->AceCount; ++i) {
        void* raw_ace = nullptr; native(GetAce(dacl, i, &raw_ace), "listener-ace");
        const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw_ace);
        need(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE, "listener-ace-kind");
        if(i) result += ',';
        result += "{\"sid\":\"" + sid_text(const_cast<DWORD*>(&ace->SidStart))
            + "\",\"mask\":" + std::to_string(ace->Mask)
            + ",\"flags\":" + std::to_string(ace->Header.AceFlags) + '}';
    }
    result += "]}";
    return result;
}
std::unique_ptr<Handle> create_pipe(const std::wstring& name, const std::string& writer,
    const std::string& client, const std::string& extra, bool extra_client_rights = false)
{
    PSECURITY_DESCRIPTOR raw = nullptr;
    native(ConvertStringSecurityDescriptorToSecurityDescriptorW(wide(descriptor_text(writer, client, extra, extra_client_rights)).c_str(),
        SDDL_REVISION_1, &raw, nullptr), "pipe-security"); Local allocation{raw};
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), raw, FALSE};
    auto handle = std::make_unique<Handle>();
    handle->value = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 5000, &attributes);
    native(handle->value != INVALID_HANDLE_VALUE, "pipe-create");
    if(!extra_client_rights) verify_pipe(handle->value, writer, client, extra);
    return handle;
}
struct IoResult { DWORD count{}; bool more{}; };
IoResult io(HANDLE pipe, unsigned operation, void* buffer, DWORD size, Clock::time_point deadline)
{
    need(Clock::now() < deadline, "deadline");
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr)); native(event.value != nullptr, "io-event");
    OVERLAPPED overlapped{}; overlapped.hEvent = event.value; DWORD count = 0;
    BOOL result = operation == 0 ? ConnectNamedPipe(pipe, &overlapped)
        : operation == 1 ? ReadFile(pipe, buffer, size, &count, &overlapped) : WriteFile(pipe, buffer, size, &count, &overlapped);
    DWORD error = result ? ERROR_SUCCESS : GetLastError();
    if(operation == 0 && error == ERROR_PIPE_CONNECTED) return {};
    if(error == ERROR_IO_PENDING) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        const DWORD wait = remaining > 0 ? WaitForSingleObject(event.value, static_cast<DWORD>(remaining)) : WAIT_TIMEOUT;
        if(wait != WAIT_OBJECT_0) {
            const BOOL cancelled = CancelIoEx(pipe, &overlapped); const DWORD cancel_error = cancelled ? 0 : GetLastError();
            // Cancelled OVERLAPPED storage cannot be released before completion.
            const DWORD drained = WaitForSingleObject(event.value, 1000);
            if(drained != WAIT_OBJECT_0) { std::cerr << "I/O cancellation did not complete\n"; TerminateProcess(GetCurrentProcess(), 91); std::terminate(); }
            const BOOL completion = GetOverlappedResult(pipe, &overlapped, &count, FALSE);
            const DWORD completion_error = completion ? 0 : GetLastError();
            std::cout << "{\"event\":\"cancellation\",\"cancel_error\":" << cancel_error << ",\"completion\":" << completion_error << "}\n";
            throw Failure{"deadline", WAIT_TIMEOUT};
        }
        result = GetOverlappedResult(pipe, &overlapped, &count, FALSE); error = result ? ERROR_SUCCESS : GetLastError();
    } else if(operation != 0 && (error == ERROR_SUCCESS || error == ERROR_MORE_DATA)) {
        // OVERLAPPED byte counts come from its completed result, including a
        // message-mode read that immediately reports ERROR_MORE_DATA.
        result = GetOverlappedResult(pipe, &overlapped, &count, FALSE); error = result ? ERROR_SUCCESS : GetLastError();
    }
    if(error != ERROR_SUCCESS && !(operation == 1 && error == ERROR_MORE_DATA)) throw Failure{"pipe-io", error};
    return {count, error == ERROR_MORE_DATA};
}
std::string read_message(HANDLE pipe, std::size_t maximum, Clock::time_point deadline, unsigned* more_count = nullptr)
{
    std::array<char, 65536> chunk{}; std::string bytes;
    for(;;) {
        const auto result = io(pipe, 1, chunk.data(), static_cast<DWORD>(chunk.size()), deadline);
        if(result.more && more_count) ++*more_count;
        need(result.count <= chunk.size() && result.count <= maximum - bytes.size(), "request-oversized");
        bytes.append(chunk.data(), result.count); if(!result.more) return bytes;
    }
}
void write_message(HANDLE pipe, std::string_view bytes, Clock::time_point deadline)
{
    need(bytes.size() <= response_limit, "write-bound");
    const auto result = io(pipe, 2, const_cast<char*>(bytes.data()), static_cast<DWORD>(bytes.size()), deadline);
    need(result.count == bytes.size(), "short-write");
}
void marker(HANDLE pipe, unsigned char value, Clock::time_point deadline)
{
    const auto bytes = read_message(pipe, 65, deadline); need(bytes.size() == 1 && static_cast<unsigned char>(bytes[0]) == value, "invalid-ack");
}
std::string identify(HANDLE pipe, const std::string& writer, bool& reverted)
{
    native(ImpersonateNamedPipeClient(pipe), "identify"); std::string sid;
    const auto revert = [] {
        if(!RevertToSelf()) { TerminateProcess(GetCurrentProcess(), 92); std::terminate(); }
        HANDLE remaining = nullptr;
        if(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &remaining)) { CloseHandle(remaining); TerminateProcess(GetCurrentProcess(), 93); std::terminate(); }
        if(GetLastError() != ERROR_NO_TOKEN) { TerminateProcess(GetCurrentProcess(), 94); std::terminate(); }
    };
    try {
        HANDLE raw = nullptr; native(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &raw), "thread-token"); Handle token(raw);
        SECURITY_IMPERSONATION_LEVEL level{}; DWORD required = 0;
        native(GetTokenInformation(token.value, TokenImpersonationLevel, &level, sizeof(level), &required), "identity-level");
        need(level == SecurityIdentification, "identity-level"); sid = token_sid(token.value);
    } catch(...) { revert(); reverted = true; throw; }
    revert(); reverted = true; need(current_sid() == writer, "writer-context"); return sid;
}
struct Writer {
    std::string bytes;
    void raw(std::string_view value) { bytes += value; }
    void integer(std::uint64_t value, unsigned count) { for(unsigned i = 0; i < count; ++i) bytes += static_cast<char>((value >> (8 * i)) & 255); }
    void array(const auto& value) { bytes.append(reinterpret_cast<const char*>(value.data()), value.size()); }
    void token(const ManagedToken& value) { array(value.generation); array(value.document); integer(value.revision, 4); array(value.commit); array(value.digest); integer(value.identity.volume, 8); array(value.identity.file); }
};
struct Reader {
    std::string_view bytes; std::size_t position{};
    std::string_view raw(std::size_t count) { need(count <= bytes.size() - position, "request-size"); auto result = bytes.substr(position, count); position += count; return result; }
    std::uint64_t integer(unsigned count) { const auto value = raw(count); std::uint64_t result = 0; for(unsigned i = 0; i < count; ++i) result |= static_cast<std::uint64_t>(static_cast<unsigned char>(value[i])) << (8 * i); return result; }
    template<std::size_t N> std::array<unsigned char, N> array() { std::array<unsigned char, N> result{}; const auto value = raw(N); std::copy(value.begin(), value.end(), result.begin()); return result; }
    ManagedToken token() { ManagedToken value; value.generation = array<16>(); value.document = array<16>(); value.revision = static_cast<std::uint32_t>(integer(4)); value.commit = array<16>(); value.digest = array<32>(); value.identity.volume = integer(8); value.identity.file = array<16>(); return value; }
};
struct Request { unsigned operation{}; ManagedId correlation{}, run{}; ManagedCommitRequest commit; };
Request parse_request(std::string_view bytes)
{
    need(bytes.size() >= 32 && bytes.size() <= request_limit, "request-size"); Reader in{bytes};
    need(in.raw(8) == "SN21SAVE", "request-magic"); need(in.integer(2) == 3, "request-version");
    Request value; value.operation = static_cast<unsigned>(in.integer(2)); need(value.operation >= 1 && value.operation <= 3, "request-operation");
    need(in.integer(4) == bytes.size() - 32, "request-length"); value.correlation = in.array<16>();
    value.run = in.array<16>(); value.commit.generation = in.array<16>(); value.commit.document = in.array<16>();
    if(value.operation == 1) need(in.position == bytes.size(), "request-open-length");
    else {
        value.commit.operation = in.array<16>(); value.commit.expected = in.token();
        const auto size = in.integer(4); need(size >= 1 && size <= simnodus::declaration_max_bytes, "request-project-size");
        need(size == bytes.size() - in.position, "request-project-length"); value.commit.project = in.raw(static_cast<std::size_t>(size));
    }
    return value;
}
std::string context_bytes(const ManagedContext& context)
{
    Writer out; out.raw("SRC1"); out.array(context.binding); out.integer(context.identity.volume, 8); out.array(context.identity.file);
    out.integer(context.policy, 4); out.integer(context.locator.size(), 4); out.raw(context.locator); return out.bytes;
}
std::string response(const Request& request, unsigned status, std::string_view payload = {})
{
    Writer out; out.raw("SN21SAVE"); out.integer(3, 2); out.integer(request.operation | 0x8000, 2);
    out.integer(4 + payload.size(), 4); out.array(request.correlation); out.integer(status, 4); out.raw(payload); return out.bytes;
}
std::string receipt_payload(const ManagedReceipt& receipt)
{
    Writer out; out.array(receipt.operation); out.token(receipt.token); out.array(receipt.request_digest); return out.bytes;
}
std::shared_ptr<const ManagedChain> chain(ManagedStore& store)
{
    auto result = store.read(); if(const auto* error = std::get_if<ManagedStoreError>(&result)) throw Failure{error->code, error->system_code};
    return std::get<std::shared_ptr<const ManagedChain>>(result);
}
void file_new(const std::string& path, std::string_view bytes)
{
    Handle file(CreateFileW(wide(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    need(file.value != INVALID_HANDLE_VALUE, "output-create"); DWORD count = 0;
    native(WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr), "output-write"); need(count == bytes.size(), "output-write");
}
std::string file_read(const std::string& path, std::size_t maximum)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate); need(file.good(), "input-open"); const auto size = file.tellg();
    need(size >= 0 && static_cast<std::uint64_t>(size) <= maximum, "input-bound"); file.seekg(0);
    std::string bytes(static_cast<std::size_t>(size), '\0'); file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())); need(file.good(), "input-read"); return bytes;
}
void fault_listener(int argc, char** argv)
{
    need(argc == 7 || argc == 8, "listener-arguments");
    const auto kind = std::string_view(argv[6]);
    need(kind == "owner" || kind == "rights" || kind == "occupied", "listener-kind");
    need((kind == "occupied") == (argc == 8), "listener-release-arguments");
    const auto writer = current_sid(), client = sid_bytes(argv[4]);
    const auto extra = std::string_view(argv[5]) == "none" ? std::string{} : sid_bytes(argv[5]);
    const auto pipe = create_pipe(pipe_name(argv[2]), writer, client, extra, kind == "rights");
    std::cout << "{\"event\":\"listener-ready\",\"kind\":\"" << kind
        << "\",\"descriptor\":" << observed_pipe_descriptor(pipe->value) << "}\n";
    file_new(argv[3], "ready\n");
    if(kind == "occupied") {
        const auto release = wide(argv[7]); const auto limit = Clock::now() + std::chrono::seconds(40);
        DWORD attributes = INVALID_FILE_ATTRIBUTES;
        while((attributes = GetFileAttributesW(release.c_str())) == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError();
            if(error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw Failure{"listener-release", error};
            need(Clock::now() < limit, "listener-release-deadline");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        need((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0, "listener-release-file");
        std::cout << "{\"event\":\"listener-result\",\"kind\":\"occupied\",\"status\":\"held-name\",\"release_observed\":true}\n";
        return;
    }
    const auto deadline = Clock::now() + std::chrono::seconds(15);
    io(pipe->value, 0, nullptr, 0, deadline);
    try {
        const auto bytes = read_message(pipe->value, request_limit, std::min(deadline, Clock::now() + std::chrono::seconds(2)));
        need(bytes.empty(), "listener-request-received");
    } catch(Failure error) {
        need(std::string_view(error.code) == "pipe-io" &&
            (error.system == ERROR_BROKEN_PIPE || error.system == ERROR_NO_DATA || error.system == ERROR_PIPE_NOT_CONNECTED),
            "listener-closure");
    }
    std::cout << "{\"event\":\"listener-result\",\"kind\":\"" << kind
        << "\",\"status\":\"closed-without-request\",\"request_bytes\":0}\n";
}
void server(int argc, char** argv, std::string_view terminal_fault = {})
{
    need(terminal_fault.empty() ? (argc == 7 || argc == 8) : argc == 8, "server-arguments");
    need(terminal_fault.empty() || terminal_fault == "terminal-close" || terminal_fault == "terminal-stall", "terminal-fault");
    const std::string root(argv[2]), leaf(argv[3]), ready(argv[4]);
    const unsigned count = static_cast<unsigned>(std::stoul(argv[5])); need(count >= 1 && count <= 32, "connection-bound");
    const unsigned drop = terminal_fault.empty() && argc == 8 ? static_cast<unsigned>(std::stoul(argv[7])) : 0;
    need(drop <= count, "drop-bound");
    const auto extra = std::string_view(argv[6]) == "none" ? std::string{} : sid_bytes(argv[6]);
    const auto writer = current_sid(); const auto name = pipe_name(leaf); auto opened = open_managed_store(root);
    if(const auto* error = std::get_if<ManagedStoreError>(&opened)) throw Failure{error->code, error->system_code};
    auto store = std::move(std::get<std::unique_ptr<ManagedStore>>(opened)); const auto allowed = store->allowed_principal(); auto initial = chain(*store);
    need(!initial->entries().empty(), "seed-required"); const auto context = initial->entries().front().decoded.record.context;
    for(const auto& entry : initial->entries()) need(entry.decoded.record.context == context, "context-rebind-unsupported");
    auto pipe = create_pipe(name, writer, allowed, extra); Writer token; token.token(initial->current());
    std::cout << "{\"event\":\"ready\",\"status\":\"ready\",\"run\":\"" << hex(store->run()) << "\",\"generation\":\"" << hex(initial->scope().generation)
        << "\",\"document\":\"" << hex(initial->scope().document) << "\",\"token\":\"" << hex(token.bytes) << "\",\"revision\":" << initial->current().revision << "}\n";
    file_new(ready, "ready\n"); const auto overall = Clock::now() + std::chrono::seconds(180);
    for(unsigned connection = 1; connection <= count; ++connection) {
        const char* stage = "connect"; bool dispatched = false, reverted = false, committed = false; std::string authenticated;
        const char* decision_code = "ok"; DWORD decision_system = 0, store_system = 0; unsigned decision_status = 1, decision_operation = 0; std::string decision_token;
        bool decision_indeterminate = false;
        try {
            io(pipe->value, 0, nullptr, 0, std::min(overall, Clock::now() + std::chrono::seconds(15)));
            const auto deadline = std::min(overall, Clock::now() + std::chrono::seconds(5)); stage = "read";
            const auto bytes = read_message(pipe->value, request_limit, deadline); stage = "identify";
            authenticated = identify(pipe->value, writer, reverted); stage = "authorize"; need(authenticated == allowed, "principal");
            stage = "frame"; auto request = parse_request(bytes); decision_operation = request.operation; need(request.run == store->run(), "request-run");
            need(request.commit.generation == initial->scope().generation, "request-generation"); need(request.commit.document == initial->scope().document, "request-document");
            unsigned status = 0; std::string payload; DWORD system = 0; const char* code = "ok"; stage = "dispatch"; dispatched = true;
            if(request.operation == 1) {
                auto current = chain(*store); const auto& record = current->entries().back().decoded.record; Writer result; result.token(current->current());
                const auto encoded_context = context_bytes(record.context); result.integer(encoded_context.size(), 4); result.raw(encoded_context);
                result.integer(record.project.size(), 4); result.raw(record.project); payload = std::move(result.bytes); Writer encoded; encoded.token(current->current()); decision_token = hex(encoded.bytes);
            } else {
                request.commit.principal_sid = authenticated; request.commit.context = context;
                if(request.operation == 2) {
                    const auto saved = store->save(request.run, request.commit);
                    if(const auto* error = std::get_if<ManagedStoreError>(&saved)) {
                        code = error->code; system = error->system_code; status = error->indeterminate ? 3u : std::string_view(error->code) == "busy" ? 2u : 1u;
                    } else { const auto& receipt = std::get<ManagedReceipt>(saved); payload = receipt_payload(receipt); committed = true; Writer encoded; encoded.token(receipt.token); decision_token = hex(encoded.bytes); }
                } else {
                    const auto found = store->reconcile(request.run, request.commit);
                    if(const auto* error = std::get_if<ManagedStoreError>(&found)) {
                        code = error->code; system = error->system_code; status = error->indeterminate ? 3u : std::string_view(error->code) == "busy" ? 2u : 1u;
                    } else {
                        const auto& lookup = std::get<ManagedReceiptLookup>(found);
                        if(const auto* receipt = std::get_if<ManagedReceipt>(&lookup)) { payload = receipt_payload(*receipt); Writer encoded; encoded.token(receipt->token); decision_token = hex(encoded.bytes); }
                        else if(std::holds_alternative<ManagedReceiptNotObserved>(lookup)) { status = 4; code = "not-observed"; }
                        else { status = 1; code = "request"; system = static_cast<DWORD>(std::get<ManagedChainError>(lookup)); }
                    }
                }
            }
            decision_code = code; store_system = system; decision_status = status; decision_indeterminate = status == 3;
            if(drop == connection && committed) { decision_code = "reply-dropped"; decision_indeterminate = true; }
            else {
                stage = "reply"; write_message(pipe->value, response(request, status, payload), deadline); stage = "ack"; marker(pipe->value, 0x7e, deadline);
                stage = "terminal";
                if(connection == 1 && !terminal_fault.empty()) {
                    need(committed, "fault-requires-committed-save");
                    decision_code = terminal_fault == "terminal-close" ? "terminal-closed" : "terminal-stalled";
                    decision_indeterminate = true;
                    if(terminal_fault == "terminal-stall") std::this_thread::sleep_until(deadline + std::chrono::milliseconds(250));
                } else {
                    write_message(pipe->value, std::string(1, '\x7f'), deadline); stage = "terminal-ack"; marker(pipe->value, 0x7f, deadline);
                    std::cout << "{\"event\":\"connection-complete\",\"connection\":" << connection << "}\n";
                }
            }
        } catch(Failure error) {
            decision_code = error.code; decision_system = error.system; decision_indeterminate = committed || decision_indeterminate;
            decision_status = decision_indeterminate ? 3u : 1u;
        }
        if(!DisconnectNamedPipe(pipe->value)) {
            const DWORD error = GetLastError(); if(error != ERROR_PIPE_NOT_CONNECTED) throw Failure{"pipe-disconnect", error};
        }
        std::cout << "{\"event\":\"decision\",\"connection\":" << connection << ",\"operation\":" << decision_operation << ",\"stage\":\"" << stage << "\",\"code\":\"" << decision_code
            << "\",\"authenticated_sid\":\"" << (authenticated.empty() ? "" : text_of(authenticated)) << "\",\"reverted\":" << (reverted ? "true" : "false") << ",\"dispatched\":" << (dispatched ? "true" : "false")
            << ",\"reply_status\":" << decision_status << ",\"system\":" << decision_system << ",\"store_system\":" << store_system << ",\"indeterminate\":" << (decision_indeterminate ? "true" : "false") << ",\"token\":\"" << decision_token << "\"}\n";
    }
    std::cout << "{\"event\":\"server-complete\",\"status\":\"served\",\"connections\":" << count << "}\n";
}
void client(int argc, char** argv, bool disconnect, bool wait_busy = false, bool idle = false)
{
    need(wait_busy ? argc == 9 : idle ? argc == 7 : (argc == 7 || argc == 8), "client-arguments");
    const auto name = pipe_name(argv[2]); const auto writer = sid_bytes(argv[3]), allowed = sid_bytes(argv[4]);
    const auto extra = std::string_view(argv[5]) == "none" ? std::string{} : sid_bytes(argv[5]); const auto bytes = file_read(argv[6], request_limit + 1);
    const char* stage = "open"; bool verified = false, sent = false; unsigned busy_retries = 0;
    try {
        if(wait_busy) {
            stage = "gate";
            std::cout << "{\"event\":\"client-ready\",\"status\":\"waiting-for-gate\",\"pid\":" << GetCurrentProcessId() << "}\n";
            const auto gate = wide(argv[8]); const auto limit = Clock::now() + std::chrono::seconds(10);
            DWORD attributes = INVALID_FILE_ATTRIBUTES;
            while((attributes = GetFileAttributesW(gate.c_str())) == INVALID_FILE_ATTRIBUTES) {
                const DWORD error = GetLastError();
                if(error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw Failure{"gate-read", error};
                need(Clock::now() < limit, "gate-deadline");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            need((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0, "gate-file");
        }
        stage = "open"; const auto open_limit = Clock::now() + std::chrono::seconds(5); Handle pipe;
        for(;;) {
            if(wait_busy) need(Clock::now() < open_limit, "pipe-open-deadline");
            pipe.value = CreateFileW(name.c_str(), client_rights, 0, nullptr, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
            if(pipe.value != INVALID_HANDLE_VALUE) break;
            const DWORD error = GetLastError();
            if(!wait_busy || error != ERROR_PIPE_BUSY) throw Failure{"pipe-open", error};
            ++busy_retries;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        stage = "verify-server"; verify_pipe(pipe.value, writer, allowed, extra); verified = true;
        DWORD mode = PIPE_READMODE_MESSAGE; native(SetNamedPipeHandleState(pipe.value, &mode, nullptr, nullptr), "pipe-mode");
        if(idle) {
            stage = "read-stall"; std::this_thread::sleep_for(std::chrono::seconds(6));
            std::cout << "{\"event\":\"client-result\",\"status\":\"held-idle\",\"server_verified\":true,\"request_sent\":false,\"indeterminate\":false}\n";
            return;
        }
        const auto deadline = Clock::now() + std::chrono::seconds(5); stage = "send"; write_message(pipe.value, bytes, deadline); sent = true;
        if(disconnect) { std::cout << "{\"event\":\"client-result\",\"status\":\"disconnected\",\"server_verified\":true,\"indeterminate\":true}\n"; return; }
        stage = "response"; const auto reply = read_message(pipe.value, response_limit, deadline);
        need(bytes.size() >= 32 && reply.size() >= 36, "response-size"); Reader in{reply};
        need(in.raw(8) == "SN21SAVE" && in.integer(2) == 3, "response-header");
        Reader request_header{bytes}; request_header.raw(10); const auto operation = static_cast<unsigned>(request_header.integer(2));
        need(operation >= 1 && operation <= 3, "response-operation");
        need(in.integer(2) == (operation | 0x8000) && in.integer(4) == reply.size() - 32, "response-header");
        need(in.raw(16) == std::string_view(bytes).substr(16, 16), "response-correlation"); const auto status = static_cast<unsigned>(in.integer(4)); need(status <= 4 && (status != 4 || operation == 3), "response-status");
        ManagedToken token; ManagedId operation_id{}; ManagedDigest digest{}; bool has_token = false;
        if(status == 0) {
            if(operation == 1) {
                token = in.token(); has_token = true; const auto size = in.integer(4); need(size >= 52 && size <= managed_context_max_bytes, "response-context-size");
                Reader context{in.raw(static_cast<std::size_t>(size))}; need(context.raw(4) == "SRC1", "response-context"); context.raw(16 + 8 + 16);
                need(context.integer(4) == 1, "response-context"); const auto locator = context.integer(4); need(locator <= 4096 && locator == context.bytes.size() - context.position, "response-context"); context.raw(static_cast<std::size_t>(locator));
                const auto project = in.integer(4); need(project >= 1 && project <= simnodus::declaration_max_bytes && project == reply.size() - in.position, "response-project"); in.raw(static_cast<std::size_t>(project));
            } else {
                need(operation == 2 || operation == 3, "response-operation"); operation_id = in.array<16>(); token = in.token(); digest = in.array<32>(); has_token = true;
                need(bytes.size() >= 96 && std::string_view(bytes).substr(80, 16) == std::string_view(reinterpret_cast<const char*>(operation_id.data()), operation_id.size()), "response-operation-id");
            }
        }
        if(has_token) {
            const auto nonzero = [](const auto& value) { return std::any_of(value.begin(), value.end(), [](unsigned char byte) { return byte != 0; }); };
            need(bytes.size() >= 80 && std::string_view(bytes).substr(48, 16) == std::string_view(reinterpret_cast<const char*>(token.generation.data()), token.generation.size())
                && std::string_view(bytes).substr(64, 16) == std::string_view(reinterpret_cast<const char*>(token.document.data()), token.document.size()), "response-token-scope");
            need(token.revision >= 1 && token.revision <= managed_revision_limit && nonzero(token.generation) && nonzero(token.document)
                && nonzero(token.commit) && nonzero(token.digest) && token.identity.volume != 0 && nonzero(token.identity.file), "response-token-fields");
        }
        need(in.position == reply.size(), "response-size");
        // Retain a validated reply even when the terminal exchange later fails.
        // A reply file alone does not establish that the client accepted a Save.
        if(argc >= 8) file_new(argv[7], reply);
        stage = "ack"; write_message(pipe.value, std::string(1, '\x7e'), deadline); stage = "terminal"; marker(pipe.value, 0x7f, deadline);
        stage = "terminal-ack"; write_message(pipe.value, std::string(1, '\x7f'), deadline); stage = "closure";
        try { read_message(pipe.value, 65, deadline); throw Failure{"extra-response"}; }
        catch(Failure error) { if(error.system != ERROR_BROKEN_PIPE && error.system != ERROR_NO_DATA && error.system != ERROR_PIPE_NOT_CONNECTED) throw; }
        Writer encoded; if(has_token) encoded.token(token);
        const char* result = status == 0 ? "accepted" : status == 1 ? "rejected" : status == 2 ? "busy" : status == 3 ? "indeterminate" : "not-observed";
        std::cout << "{\"event\":\"client-result\",\"status\":\"" << result << "\",\"operation\":" << operation << ",\"reply_status\":" << status << ",\"server_verified\":true,\"token\":\"" << hex(encoded.bytes)
            << "\",\"revision\":" << token.revision << ",\"operation_id\":\"" << hex(operation_id) << "\",\"request_digest\":\"" << hex(digest) << "\",\"response_bytes\":" << reply.size();
        if(wait_busy) std::cout << ",\"pipe_busy_retries\":" << busy_retries;
        std::cout << "}\n";
    } catch(Failure error) {
        std::cout << "{\"event\":\"client-result\",\"status\":\"failed\",\"stage\":\"" << stage << "\",\"code\":\"" << error.code << "\",\"system\":" << error.system
            << ",\"server_verified\":" << (verified ? "true" : "false") << ",\"request_sent\":" << (sent ? "true" : "false") << ",\"indeterminate\":" << (sent ? "true" : "false");
        if(wait_busy) std::cout << ",\"pipe_busy_retries\":" << busy_retries;
        std::cout << "}\n";
    }
}
void parser_checks()
{
    Writer out; out.raw("SN21SAVE"); out.integer(3, 2); out.integer(1, 2); out.integer(48, 4); out.array(ManagedId{}); out.array(ManagedId{}); out.array(ManagedId{}); out.array(ManagedId{});
    need(parse_request(out.bytes).operation == 1, "self-open"); unsigned checks = 1;
    for(std::size_t size = 0; size < out.bytes.size(); ++size) { bool rejected = false; try { parse_request(std::string_view(out.bytes).substr(0, size)); } catch(Failure) { rejected = true; } need(rejected, "self-truncation"); ++checks; }
    for(const std::size_t offset : {std::size_t{0}, std::size_t{8}, std::size_t{10}, std::size_t{12}}) {
        auto mutated = out.bytes; mutated[offset] ^= 0x40; bool rejected = false; try { parse_request(mutated); } catch(Failure) { rejected = true; } need(rejected, "self-header"); ++checks;
    }
    const auto refuse = [&](std::string_view bytes) {
        bool rejected = false; try { parse_request(bytes); } catch(Failure) { rejected = true; } need(rejected, "self-refusal"); ++checks;
    };
    auto trailing = out.bytes + 'x'; refuse(trailing);
    for(unsigned operation : {2u, 3u}) {
        Writer body; body.array(ManagedId{}); body.array(ManagedId{}); body.array(ManagedId{}); body.array(ManagedId{}); body.token(ManagedToken{}); body.integer(1, 4); body.raw("x");
        Writer frame; frame.raw("SN21SAVE"); frame.integer(3, 2); frame.integer(operation, 2); frame.integer(body.bytes.size(), 4); frame.array(ManagedId{}); frame.raw(body.bytes);
        const auto parsed = parse_request(frame.bytes); need(parsed.operation == operation && parsed.commit.project == "x", "self-save"); ++checks;
        for(std::size_t size = 32; size < frame.bytes.size(); ++size) refuse(std::string_view(frame.bytes).substr(0, size));
        auto empty = frame.bytes; empty[204] = 0; refuse(empty);
        auto excess = frame.bytes + 'x'; refuse(excess);
        auto overflow = frame.bytes; std::fill(overflow.begin() + 204, overflow.begin() + 208, static_cast<char>(0xff)); refuse(overflow);
    }
    Writer large_body; large_body.array(ManagedId{}); large_body.array(ManagedId{}); large_body.array(ManagedId{}); large_body.array(ManagedId{}); large_body.token(ManagedToken{});
    large_body.integer(simnodus::declaration_max_bytes, 4); large_body.raw(std::string(simnodus::declaration_max_bytes, 'x'));
    Writer large; large.raw("SN21SAVE"); large.integer(3, 2); large.integer(2, 2); large.integer(large_body.bytes.size(), 4); large.array(ManagedId{}); large.raw(large_body.bytes);
    need(parse_request(large.bytes).commit.project.size() == simnodus::declaration_max_bytes, "self-maximum"); ++checks; refuse(large.bytes + 'x');
    std::cout << "{\"status\":\"parser-checked\",\"checks\":" << checks << "}\n";
}
void transport_checks()
{
    // Same-identity I/O only. This does not authenticate another principal or
    // touch a managed store, provision an account, or grant host permissions.
    const auto writer = current_sid(), dummy = sid_bytes("S-1-5-21-1-2-3-2147483647");
    const auto tag = static_cast<std::uint64_t>(GetCurrentProcessId()) ^ GetTickCount64(); Writer tag_bytes; tag_bytes.integer(tag, 6);
    const auto name = pipe_name("SN021Save-" + hex(tag_bytes.bytes)); auto pipe = create_pipe(name, writer, dummy, {});
    DWORD occupied_system = 0;
    try { auto occupied = create_pipe(name, writer, dummy, {}); }
    catch(Failure error) {
        if(std::string_view(error.code) == "pipe-create") occupied_system = error.system;
    }
    need(occupied_system != 0, "transport-occupied-name");
    std::string maximum(request_limit, '\0');
    for(std::size_t i = 0; i < maximum.size(); ++i) maximum[i] = static_cast<char>(i % 251);
    unsigned server_more = 0, client_more = 0; bool oversize_refused = false; std::exception_ptr server_error, client_error;
    std::thread worker([&] {
        const char* stage = "connect"; unsigned observed_round = 0;
        try {
            for(unsigned round = 0; round < 2; ++round) {
                observed_round = round;
                stage = "connect"; const auto deadline = Clock::now() + std::chrono::seconds(5); io(pipe->value, 0, nullptr, 0, deadline); stage = "read";
                if(round == 0) {
                    const auto bytes = read_message(pipe->value, request_limit, deadline, &server_more);
                    need(bytes == maximum, "transport-request-bytes"); stage = "reply"; write_message(pipe->value, bytes, deadline);
                } else {
                    try { read_message(pipe->value, request_limit, deadline, &server_more); throw Failure{"transport-oversize-accepted"}; }
                    catch(Failure error) { need(std::string_view(error.code) == "request-oversized", "transport-oversize-code"); oversize_refused = true; }
                    stage = "reply"; write_message(pipe->value, "!", deadline);
                }
                stage = "ack"; marker(pipe->value, 0x7e, deadline); stage = "disconnect"; native(DisconnectNamedPipe(pipe->value), "transport-disconnect");
            }
        } catch(Failure error) { server_error = std::current_exception(); std::cout << "{\"event\":\"transport-failure\",\"side\":\"server\",\"stage\":\"" << stage << "\",\"round\":" << observed_round << ",\"more\":" << server_more << ",\"code\":\"" << error.code << "\",\"system\":" << error.system << "}\n"; DisconnectNamedPipe(pipe->value); }
        catch(...) { server_error = std::current_exception(); DisconnectNamedPipe(pipe->value); }
    });
    const char* stage = "open";
    try {
        for(unsigned round = 0; round < 2; ++round) {
            stage = "open";
            Handle client_pipe(CreateFileW(name.c_str(), client_rights, 0, nullptr, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
            native(client_pipe.value != INVALID_HANDLE_VALUE, "transport-client-open"); DWORD mode = PIPE_READMODE_MESSAGE;
            native(SetNamedPipeHandleState(client_pipe.value, &mode, nullptr, nullptr), "transport-mode");
            const auto deadline = Clock::now() + std::chrono::seconds(5); const auto sent = round == 0 ? maximum : maximum + 'x';
            stage = "send"; write_message(client_pipe.value, sent, deadline); stage = "reply"; const auto reply = read_message(client_pipe.value, response_limit, deadline, &client_more);
            need(reply == (round == 0 ? maximum : std::string{"!"}), "transport-reply-bytes");
            stage = "ack"; write_message(client_pipe.value, std::string(1, '\x7e'), deadline); stage = "closure";
            try { read_message(client_pipe.value, 65, deadline); throw Failure{"transport-extra-response"}; }
            catch(Failure error) { if(error.system != ERROR_BROKEN_PIPE && error.system != ERROR_NO_DATA && error.system != ERROR_PIPE_NOT_CONNECTED) throw; }
        }
    } catch(...) { client_error = std::current_exception(); std::cout << "{\"event\":\"transport-failure\",\"side\":\"client\",\"stage\":\"" << stage << "\"}\n"; CancelIoEx(pipe->value, nullptr); }
    worker.join(); if(client_error) std::rethrow_exception(client_error); if(server_error) std::rethrow_exception(server_error);
    need(oversize_refused && server_more >= 2 && client_more >= 1, "transport-more-data");
    std::cout << "{\"status\":\"transport-checked\",\"scope\":\"same-identity-io-only\",\"maximum_bytes\":" << request_limit
        << ",\"oversize_bytes\":" << request_limit + 1 << ",\"server_more_data\":" << server_more << ",\"client_more_data\":" << client_more
        << ",\"oversize_refused\":true,\"occupied_name_system\":" << occupied_system << "}\n";
}
}
int main(int argc, char** argv)
{
    if(argc >= 3 && std::string_view(argv[1]) == "--report") {
        const auto redirect = [](const std::string& path, FILE* stream) {
            HANDLE raw = CreateFileW(wide(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if(raw == INVALID_HANDLE_VALUE) return false;
            const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(raw), _O_WRONLY | _O_BINARY);
            if(descriptor < 0) { CloseHandle(raw); return false; }
            const bool copied = _dup2(descriptor, _fileno(stream)) == 0; _close(descriptor); return copied;
        };
        if(!redirect(argv[2], stdout) || !redirect(std::string(argv[2]) + ".stderr", stderr)) return 3;
        argc -= 2; argv += 2;
    }
    std::cout << std::unitbuf;
    try {
        need(argc >= 2, "arguments"); const std::string_view mode(argv[1]);
        if(mode == "parser-check") { need(argc == 2, "arguments"); parser_checks(); }
        else if(mode == "transport-check") { need(argc == 2, "arguments"); transport_checks(); }
        else if(mode == "server") server(argc, argv);
        else if(mode == "server-fault") { need(argc == 8, "server-arguments"); server(argc, argv, argv[7]); }
        else if(mode == "fault-listener") fault_listener(argc, argv);
        else if(mode == "client" || mode == "client-disconnect") client(argc, argv, mode == "client-disconnect");
        else if(mode == "client-wait") client(argc, argv, false, true);
        else if(mode == "client-idle") client(argc, argv, false, false, true);
        else throw Failure{"mode"};
        return 0;
    } catch(Failure error) { std::cout << "{\"status\":\"fixture-failed\",\"code\":\"" << error.code << "\",\"system\":" << error.system << "}\n"; return 2; }
    catch(const std::exception&) { std::cout << "{\"status\":\"fixture-failed\",\"code\":\"exception\"}\n"; return 2; }
}
