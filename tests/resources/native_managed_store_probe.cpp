// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Manual disposable-VM probe. Not registered as an automatic CTest.
#include "platform/windows/managed_store.hpp"
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <filesystem>
#include <cstdio>
#include <io.h>
#include <fcntl.h>

namespace {
using namespace simnodus::experimental;
std::string hook, action;
void require(bool value, const char* code) { if(!value) throw std::runtime_error(code); }
struct Owned {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Owned() { if(value != INVALID_HANDLE_VALUE && value) CloseHandle(value); }
    Owned(const Owned&) = delete;
    explicit Owned(HANDLE h) : value(h) {}
};
struct Local {
    void* value{};
    ~Local() { if(value) LocalFree(value); }
};
std::wstring wide(const std::string& value) { return {value.begin(), value.end()}; }
std::string hex(const auto& bytes)
{
    std::string result;
    for(unsigned char b : bytes) { result += "0123456789abcdef"[b >> 4]; result += "0123456789abcdef"[b & 15]; }
    return result;
}
ManagedId id(unsigned char value) { ManagedId result{}; result.fill(value); return result; }
ManagedId random_id()
{
    ManagedId result{};
    require(BCryptGenRandom(nullptr, result.data(), static_cast<ULONG>(result.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0, "random");
    return result;
}
std::string sid_bytes(const std::wstring& text)
{
    PSID sid = nullptr; require(ConvertStringSidToSidW(text.c_str(), &sid) != FALSE, "sid"); Local memory{sid};
    return {static_cast<const char*>(sid), GetLengthSid(sid)};
}
std::string current_sid()
{
    HANDLE raw = nullptr; require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw) != FALSE, "token"); Owned token(raw);
    alignas(TOKEN_USER) std::array<unsigned char, 1024> buffer{}; DWORD needed = 0;
    require(GetTokenInformation(token.value, TokenUser, buffer.data(), static_cast<DWORD>(buffer.size()), &needed) != FALSE, "token");
    LPWSTR text = nullptr;
    require(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &text) != FALSE, "sid");
    Local memory{text}; std::string value;
    for(const auto c : std::wstring_view(text)) { require(c > 0 && c < 128, "sid-text"); value += static_cast<char>(c); }
    return value;
}
ManagedIdentity identity(const std::wstring& path, bool directory)
{
    Owned file(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0), nullptr));
    require(file.value != INVALID_HANDLE_VALUE, "identity-open");
    FILE_ID_INFO info{}; require(GetFileInformationByHandleEx(file.value, FileIdInfo, &info, sizeof(info)) != FALSE, "identity");
    ManagedIdentity result{info.VolumeSerialNumber, {}};
    std::copy(std::begin(info.FileId.Identifier), std::end(info.FileId.Identifier), result.file.begin()); return result;
}
void create(const std::wstring& path, bool directory, const std::wstring& writer, const wchar_t* rights, std::string_view bytes = {})
{
    const auto text = L"O:BAD:P(A;;0x1f01ff;;;SY)(A;;0x1f01ff;;;BA)(A;;" + std::wstring(rights) + L";;;" + writer + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    require(ConvertStringSecurityDescriptorToSecurityDescriptorW(text.c_str(), SDDL_REVISION_1, &descriptor, nullptr) != FALSE, "descriptor");
    Local memory{descriptor}; SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    if(directory) require(CreateDirectoryW(path.c_str(), &attributes) != FALSE, "create-directory");
    else {
        Owned file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        require(file.value != INVALID_HANDLE_VALUE, "create-file");
        DWORD written = 0;
        require(WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE
            && written == bytes.size() && FlushFileBuffers(file.value), "write-file");
    }
}
void provision(const std::string& leaf, const std::wstring& writer, const std::wstring& client)
{
    // Defense against accidentally invoking the manual fixture on the host.
    std::array<wchar_t, 256> manufacturer{}; DWORD size = sizeof(manufacturer);
    require(RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer",
        RRF_RT_REG_SZ, nullptr, manufacturer.data(), &size) == ERROR_SUCCESS
        && std::wstring_view(manufacturer.data()).find(L"VMware") != std::wstring_view::npos, "vmware-only");
    require(leaf.size() == 25 && leaf.starts_with("SN021Managed-")
        && std::all_of(leaf.begin() + 13, leaf.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "root-name");
    const auto root = L"C:\\" + wide(leaf), directory = root + L"\\document", lock = directory + L"\\writer.lock";
    create(root, true, writer, L"0x1200a9"); create(directory, true, writer, L"0x1200ab"); create(lock, false, writer, L"0x120089");
    std::string bytes = "SNST0001";
    const auto append = [&](const auto& value) { bytes.append(reinterpret_cast<const char*>(value.data()), value.size()); };
    const auto integer = [&](std::uint64_t value, unsigned count) { for(unsigned i = 0; i < count; ++i) bytes += static_cast<char>((value >> (8 * i)) & 255); };
    append(random_id()); append(random_id());
    for(const auto& [path, is_directory] : {std::pair{root, true}, {directory, true}, {lock, false}}) {
        const auto file = identity(path, is_directory); integer(file.volume, 8); append(file.file);
    }
    const auto writer_bytes = sid_bytes(writer), client_bytes = sid_bytes(client);
    integer(writer_bytes.size(), 4); integer(client_bytes.size(), 4); bytes += writer_bytes; bytes += client_bytes;
    create(root + L"\\generation.manifest", false, writer, L"0x120089", bytes);
    std::cout << "{\"status\":\"provisioned\",\"root\":\"" << leaf << "\"}\n";
}
void error(const ManagedStoreError& value)
{
    std::cout << "{\"status\":\"rejected\",\"code\":\"" << value.code << "\",\"system\":" << value.system_code
        << ",\"indeterminate\":" << (value.indeterminate ? "true" : "false") << "}\n";
}
std::shared_ptr<const ManagedChain> read(ManagedStore& store)
{
    const auto result = store.read();
    if(const auto* rejected = std::get_if<ManagedStoreError>(&result)) { error(*rejected); throw std::runtime_error("read-rejected"); }
    return std::get<std::shared_ptr<const ManagedChain>>(result);
}
ManagedCommitRequest request(ManagedStore& store, const ManagedChain& chain, const std::string& project,
    const std::filesystem::path& resource_root, std::uint32_t previous)
{
    const auto expected = previous ? chain.entries().at(previous - 1).token : ManagedToken{};
    auto bytes = project; if(previous % 2) bytes += ' ';
    return {chain.scope().generation, chain.scope().document, id(static_cast<unsigned char>(previous + 1)),
        store.allowed_principal(), expected, {id(42), identity(resource_root.wstring(), true), 1, resource_root.string()}, std::move(bytes)};
}
ManagedReceipt saved(ManagedStore& store, const ManagedCommitRequest& value)
{
    const auto result = store.save(store.run(), value);
    if(const auto* rejected = std::get_if<ManagedStoreError>(&result)) { error(*rejected); throw std::runtime_error("save-rejected"); }
    return std::get<ManagedReceipt>(result);
}
void receipt(const ManagedReceipt& value)
{
    std::cout << "{\"status\":\"committed\",\"revision\":" << value.token.revision << ",\"digest\":\""
        << hex(value.token.digest) << "\",\"file\":\"" << hex(value.token.identity.file) << "\"}\n";
}
void attack(const std::string& root_leaf, const std::string& stage, bool writer_lock_only)
{
    require(root_leaf.size() == 25 && root_leaf.starts_with("SN021Managed-")
        && std::all_of(root_leaf.begin() + 13, root_leaf.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "root-name");
    const auto root = L"C:\\" + wide(root_leaf), directory = root + L"\\document";
    std::vector<std::wstring> paths{directory + L"\\writer.lock"};
    if(!writer_lock_only) {
        paths.push_back(root + L"\\generation.manifest"); paths.push_back(directory + L"\\00000001.commit");
        if(stage != "none") {
            require(stage.size() == 42 && stage.starts_with("stage-") && stage.ends_with(".tmp")
                && std::all_of(stage.begin() + 6, stage.begin() + 38, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "stage-name");
            paths.push_back(directory + L"\\" + wide(stage));
        }
    }
    unsigned denied = 0;
    for(const auto& path : paths) {
        for(const auto access : {GENERIC_WRITE, DELETE, WRITE_DAC, WRITE_OWNER}) {
            Owned file(CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            const auto code = GetLastError();
            require(file.value == INVALID_HANDLE_VALUE && code == ERROR_ACCESS_DENIED, "mutation-open-not-denied"); ++denied;
        }
        require(DeleteFileW(path.c_str()) == FALSE && GetLastError() == ERROR_ACCESS_DENIED, "delete-not-denied"); ++denied;
        require(MoveFileExW(path.c_str(), (path + L".moved").c_str(), 0) == FALSE
            && GetLastError() == ERROR_ACCESS_DENIED, "rename-not-denied"); ++denied;
    }
    if(!writer_lock_only) {
        Owned file(CreateFileW((directory + L"\\client.tmp").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        require(file.value == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "create-not-denied"); ++denied;
        require(MoveFileExW(directory.c_str(), (root + L"\\moved").c_str(), 0) == FALSE
            && GetLastError() == ERROR_ACCESS_DENIED, "directory-rename-not-denied"); ++denied;
        require(CreateHardLinkW((directory + L"\\client.link").c_str(), (directory + L"\\00000001.commit").c_str(), nullptr) == FALSE
            && GetLastError() == ERROR_ACCESS_DENIED, "link-not-denied"); ++denied;
    }
    std::cout << "{\"status\":\"mutations-denied\",\"count\":" << denied << "}\n";
}
}
namespace simnodus::experimental {
bool managed_store_test_boundary(const char* phase)
{
    if(hook != phase) return true;
    std::cout << "{\"barrier\":\"" << phase << "\"}\n";
    if(action == "hold") Sleep(45000);
    return false;
}
}
int main(int argc, char** argv)
{
    using namespace simnodus::experimental;
    // Write reports directly; cross-identity PowerShell pipe redirection can
    // leave the coordinator waiting after the native process has reported failure.
    if(argc >= 3 && std::string_view(argv[1]) == "--report") {
        const std::string path(argv[2]);
        const auto report = [](const std::string& name, FILE* stream) {
            HANDLE handle = CreateFileA(name.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if(handle == INVALID_HANDLE_VALUE) return false;
            const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_WRONLY | _O_BINARY);
            if(descriptor < 0) { CloseHandle(handle); return false; }
            const bool copied = _dup2(descriptor, _fileno(stream)) == 0;
            _close(descriptor); return copied;
        };
        if(!report(path, stdout) || !report(path + ".stderr", stderr)) return 3;
        argc -= 2; argv += 2;
    }
    std::cout << std::unitbuf;
    try {
        require(argc >= 3, "arguments");
        const std::string mode(argv[1]), root(argv[2]);
        std::cout << "{\"event\":\"token\",\"sid\":\"" << current_sid() << "\",\"pid\":" << GetCurrentProcessId() << "}\n";
        if(mode == "report-check") {
            require(root == "inert", "arguments");
            std::cout << "{\"barrier\":\"report-readable\"}\n";
            Sleep(3000); return 0;
        }
        if(mode == "provision") { require(argc == 5, "arguments"); provision(root, wide(argv[3]), wide(argv[4])); return 0; }
        if(mode == "attack" || mode == "lockattack") {
            require(argc == 4, "arguments"); attack(root, argv[3], mode == "lockattack"); return 0;
        }
        if(argc == 6) { hook = argv[4]; action = argv[5]; require(action == "fail" || action == "hold", "action"); }
        auto opened = open_managed_store(root);
        if(const auto* rejected = std::get_if<ManagedStoreError>(&opened)) { error(*rejected); return 0; }
        auto store = std::move(std::get<std::unique_ptr<ManagedStore>>(opened));
        auto chain = read(*store);
        std::cout << "{\"status\":\"opened\",\"revision\":" << chain->current().revision << ",\"run\":\"" << hex(store->run()) << "\"}\n";
        if(mode == "open") return 0;
        require(argc >= 4, "arguments");
        const auto project_path = std::filesystem::path(argv[3]);
        std::ifstream file(project_path, std::ios::binary | std::ios::ate);
        require(file.good() && file.tellg() > 0 && file.tellg() <= 71680, "project-input"); file.seekg(0);
        std::string project(std::istreambuf_iterator<char>{file}, {}); project.resize(71680, ' ');
        const auto resource_root = project_path.parent_path();
        if(mode == "write") {
            const auto result = store->save(store->run(), request(*store, *chain, project, resource_root, chain->current().revision));
            if(const auto* rejected = std::get_if<ManagedStoreError>(&result)) error(*rejected);
            else receipt(std::get<ManagedReceipt>(result));
        } else if(mode == "reconcile-second") {
            require(chain->entries().size() >= 3, "later-revision-required");
            const auto result = store->reconcile(store->run(), request(*store, *chain, project, resource_root, 1));
            require(std::holds_alternative<ManagedReceiptLookup>(result), "reconcile-error");
            const auto& found = std::get<ManagedReceiptLookup>(result);
            require(std::holds_alternative<ManagedReceipt>(found) && std::get<ManagedReceipt>(found).token.revision == 2, "old-receipt");
            std::cout << "{\"status\":\"older-receipt-reconciled\",\"revision\":2}\n";
        } else if(mode == "sequence") {
            require(chain->entries().empty(), "fresh-sequence");
            const auto initial = request(*store, *chain, project, resource_root, 0);
            const auto first = saved(*store, initial);
            for(unsigned i = 1; i < 64; ++i) {
                chain = read(*store);
                const auto next = request(*store, *chain, project, resource_root, i);
                const auto written = saved(*store, next); require(written.token.revision == i + 1, "revision");
                if(i == 2) {
                    const auto aba = read(*store);
                    require(aba->entries()[0].decoded.record.project == aba->entries()[2].decoded.record.project
                        && first.token != written.token, "aba");
                    auto stale = initial; stale.operation = id(200); stale.expected = first.token;
                    const auto result = store->save(store->run(), stale);
                    require(std::holds_alternative<ManagedStoreError>(result)
                        && std::get<ManagedStoreError>(result).system_code == static_cast<DWORD>(ManagedChainError::stale), "stale");
                }
            }
            chain = read(*store);
            const auto full = store->save(store->run(), request(*store, *chain, project, resource_root, 64));
            require(std::holds_alternative<ManagedStoreError>(full)
                && std::get<ManagedStoreError>(full).system_code == static_cast<DWORD>(ManagedChainError::capacity), "capacity");
            require(saved(*store, initial).token == first.token, "old-receipt-at-capacity");
            auto collision = initial; collision.project += ' ';
            const auto conflicted = store->save(store->run(), collision);
            require(std::holds_alternative<ManagedStoreError>(conflicted)
                && std::get<ManagedStoreError>(conflicted).system_code == static_cast<DWORD>(ManagedChainError::operation_conflict), "operation-conflict");
            auto wrong = initial; wrong.principal_sid.back() ^= 1;
            require(std::holds_alternative<ManagedStoreError>(store->save(store->run(), wrong)), "wrong-principal");
            const auto old_run = store->run(); store.reset();
            auto reopened = open_managed_store(root); require(std::holds_alternative<std::unique_ptr<ManagedStore>>(reopened), "reopen");
            store = std::move(std::get<std::unique_ptr<ManagedStore>>(reopened));
            require(store->run() != old_run && read(*store)->current().revision == 64, "restart");
            require(std::holds_alternative<ManagedStoreError>(store->save(old_run, initial)), "old-run");
            require(saved(*store, initial).token == first.token, "receipt-after-restart");
            std::cout << "{\"status\":\"sequence-observed\",\"revisions\":64,\"aba\":true,\"capacity\":true,\"older_receipt\":true,\"run_fencing\":true}\n";
        } else throw std::runtime_error("mode");
        return 0;
    } catch(const std::exception& value) {
        std::cerr << value.what() << '\n'; std::cout << "{\"status\":\"fixture-failed\"}\n"; return 2;
    }
}
