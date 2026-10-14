// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "platform/windows/managed_store.hpp"
#include "platform/windows/resource_handles_internal.hpp"
#include <aclapi.h>
#include <sddl.h>
#include <bcrypt.h>
#include <cstring>
#include <mutex>
#include <set>

extern "C" NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(
    HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

namespace simnodus::experimental {
namespace {
using resource_platform::detail::Handle;
constexpr DWORD full = 0x1f01ff, root_rights = 0x1200a9, directory_rights = 0x1200ab;
constexpr DWORD read_rights = 0x120089, record_rights = 0x13019f;
struct LocalMemory {
    void* value{};
    ~LocalMemory() { if(value) LocalFree(value); }
};
[[noreturn]] void fail(const char* code, DWORD system = 0) { throw ManagedStoreError{code, system}; }
void require(bool value, const char* code) { if(!value) fail(code); }
void check(BOOL value, const char* code) { if(!value) fail(code, GetLastError()); }
void boundary(const char* phase)
{
#ifdef SIMNODUS_MANAGED_STORE_TEST_HOOKS
    if(!managed_store_test_boundary(phase)) fail("injected");
#else
    (void)phase;
#endif
}
bool hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }
bool root_name(const std::string& name)
{
    return name.size() == 25 && name.starts_with("SN021Managed-")
        && std::all_of(name.begin() + 13, name.end(), hex);
}
std::wstring wide(const std::string& ascii) { return {ascii.begin(), ascii.end()}; }
ManagedId random_id()
{
    ManagedId result{};
    const auto status = BCryptGenRandom(nullptr, result.data(), static_cast<ULONG>(result.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status < 0) fail("random", static_cast<DWORD>(status));
    require(std::any_of(result.begin(), result.end(), [](auto b) { return b != 0; }), "random-zero");
    return result;
}
std::wstring sid_text(PSID sid)
{
    LPWSTR raw = nullptr;
    check(ConvertSidToStringSidW(sid, &raw), "sid"); LocalMemory memory{raw};
    return raw;
}
std::string process_sid()
{
    HANDLE raw = nullptr; check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw), "token");
    Handle token(raw);
    TOKEN_ELEVATION elevation{}; DWORD needed = 0;
    check(GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &needed), "token");
    require(!elevation.TokenIsElevated, "ordinary-writer-required");
    std::array<unsigned char, SECURITY_MAX_SID_SIZE> admin{}; DWORD size = static_cast<DWORD>(admin.size());
    check(CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin.data(), &size), "token");
    BOOL member = FALSE; check(CheckTokenMembership(nullptr, admin.data(), &member), "token");
    require(!member, "ordinary-writer-required");
    alignas(TOKEN_PRIVILEGES) std::array<unsigned char, 4096> privileges{};
    check(GetTokenInformation(token.get(), TokenPrivileges, privileges.data(), static_cast<DWORD>(privileges.size()), &needed), "token-privileges");
    const auto* held = reinterpret_cast<const TOKEN_PRIVILEGES*>(privileges.data());
    require(held->PrivilegeCount <= (privileges.size() - offsetof(TOKEN_PRIVILEGES, Privileges)) / sizeof(LUID_AND_ATTRIBUTES), "token-privileges");
    for(const auto name : {L"SeTakeOwnershipPrivilege", L"SeRestorePrivilege", L"SeBackupPrivilege", L"SeDebugPrivilege",
        L"SeTcbPrivilege", L"SeImpersonatePrivilege", L"SeAssignPrimaryTokenPrivilege", L"SeCreateTokenPrivilege"}) {
        LUID dangerous{}; check(LookupPrivilegeValueW(nullptr, name, &dangerous), "token-privileges");
        for(DWORD i = 0; i < held->PrivilegeCount; ++i)
            require(held->Privileges[i].Luid.LowPart != dangerous.LowPart || held->Privileges[i].Luid.HighPart != dangerous.HighPart,
                "privileged-writer");
    }
    alignas(TOKEN_USER) std::array<unsigned char, 1024> buffer{};
    check(GetTokenInformation(token.get(), TokenUser, buffer.data(), static_cast<DWORD>(buffer.size()), &needed), "token");
    const auto sid = reinterpret_cast<const TOKEN_USER*>(buffer.data())->User.Sid;
    require(IsValidSid(sid) != FALSE && GetLengthSid(sid) <= SECURITY_MAX_SID_SIZE, "sid");
    return {static_cast<const char*>(sid), GetLengthSid(sid)};
}
std::wstring binary_sid_text(const std::string& sid)
{
    require(sid.size() >= 12 && sid.size() <= SECURITY_MAX_SID_SIZE, "sid");
    auto* value = const_cast<char*>(sid.data());
    // Validate the byte count before asking Windows to walk subauthorities.
    require(static_cast<unsigned char>(sid[0]) == 1 && static_cast<unsigned char>(sid[1]) <= 15
        && sid.size() == 8u + 4u * static_cast<unsigned char>(sid[1]), "sid");
    require(IsValidSid(value) != FALSE, "sid");
    return sid_text(value);
}
void security(HANDLE handle, const std::wstring& writer, bool writer_owner, DWORD rights)
{
    PSID owner = nullptr; PACL acl = nullptr; PSECURITY_DESCRIPTOR raw = nullptr;
    const auto status = GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, nullptr, &acl, nullptr, &raw);
    if(status != ERROR_SUCCESS) fail("security-read", status);
    LocalMemory memory{raw}; SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
    check(GetSecurityDescriptorControl(raw, &control, &revision), "security-control");
    require(owner && sid_text(owner) == (writer_owner ? writer : L"S-1-5-32-544")
        && (control & SE_DACL_PROTECTED) && acl && acl->AceCount == 3, "security-shape");
    std::set<std::wstring> seen;
    for(DWORD i = 0; i < acl->AceCount; ++i) {
        void* entry = nullptr; check(GetAce(acl, i, &entry), "security-ace");
        const auto* ace = static_cast<const ACCESS_ALLOWED_ACE*>(entry);
        require(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && ace->Header.AceFlags == 0, "security-ace");
        const auto principal = sid_text(const_cast<DWORD*>(&ace->SidStart));
        const auto expected = principal == writer ? rights : full;
        require((principal == writer || principal == L"S-1-5-18" || principal == L"S-1-5-32-544")
            && ace->Mask == expected && seen.insert(principal).second, "security-rights");
    }
}
void volume_security(HANDLE handle)
{
    PSID owner = nullptr; PACL acl = nullptr; PSECURITY_DESCRIPTOR raw = nullptr;
    const auto status = GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, nullptr, &acl, nullptr, &raw);
    if(status) fail("volume-security", status);
    LocalMemory memory{raw};
    const std::wstring installer = L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";
    const auto privileged = [&](const std::wstring& sid) {
        return sid == L"S-1-5-18" || sid == L"S-1-5-32-544" || sid == installer;
    };
    require(owner && privileged(sid_text(owner)) && acl, "volume-owner");
    for(DWORD i = 0; i < acl->AceCount; ++i) {
        void* entry = nullptr; check(GetAce(acl, i, &entry), "volume-ace");
        const auto* ace = static_cast<const ACCESS_ALLOWED_ACE*>(entry);
        if(ace->Header.AceFlags & INHERIT_ONLY_ACE) continue;
        require(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE || ace->Header.AceType == ACCESS_DENIED_ACE_TYPE, "volume-ace");
        if(ace->Header.AceType == ACCESS_DENIED_ACE_TYPE) continue;
        if(privileged(sid_text(const_cast<DWORD*>(&ace->SidStart)))) continue;
        constexpr DWORD mutation = DELETE | WRITE_DAC | WRITE_OWNER | FILE_DELETE_CHILD
            | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | GENERIC_ALL | GENERIC_WRITE;
        require((ace->Mask & mutation) == 0, "volume-mutation");
    }
}
ManagedIdentity identity(HANDLE handle, bool directory)
{
    resource_platform::detail::inspect(handle, directory, 0);
    FILE_ID_INFO info{}; check(GetFileInformationByHandleEx(handle, FileIdInfo, &info, sizeof(info)), "identity");
    ManagedIdentity result{info.VolumeSerialNumber, {}};
    std::copy(std::begin(info.FileId.Identifier), std::end(info.FileId.Identifier), result.file.begin());
    return result;
}
Handle volume()
{
    require(GetDriveTypeW(L"C:\\") == DRIVE_FIXED, "volume-type");
    std::array<wchar_t, 1024> mapping{};
    check(QueryDosDeviceW(L"C:", mapping.data(), static_cast<DWORD>(mapping.size())) != 0, "volume-map");
    const std::wstring path(mapping.data()), prefix = L"\\Device\\HarddiskVolume";
    require(path.starts_with(prefix) && path.size() > prefix.size()
        && std::all_of(path.begin() + static_cast<std::ptrdiff_t>(prefix.size()), path.end(),
            [](wchar_t c) { return c >= L'0' && c <= L'9'; }), "volume-map");
    Handle result(CreateFileW((L"\\\\?\\GLOBALROOT" + path + L"\\").c_str(),
        READ_CONTROL | SYNCHRONIZE | FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OPEN_NO_RECALL, nullptr));
    if(result.get() == INVALID_HANDLE_VALUE) fail("volume-open", GetLastError());
    identity(result.get(), true); volume_security(result.get());
    std::array<wchar_t, 32> filesystem{};
    check(GetVolumeInformationByHandleW(result.get(), nullptr, 0, nullptr, nullptr, nullptr,
        filesystem.data(), static_cast<DWORD>(filesystem.size())), "filesystem");
    require(std::wstring_view(filesystem.data()) == L"NTFS", "filesystem");
    return result;
}
Handle child(HANDLE parent, const std::wstring& name, bool directory, DWORD access, DWORD share,
    ULONG disposition = FILE_OPEN, PSECURITY_DESCRIPTOR security_descriptor = nullptr)
{
    UNICODE_STRING text{}; text.Buffer = const_cast<wchar_t*>(name.data());
    text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t)); text.MaximumLength = text.Length;
    OBJECT_ATTRIBUTES attributes{}; attributes.Length = sizeof(attributes); attributes.RootDirectory = parent;
    attributes.ObjectName = &text; attributes.Attributes = OBJ_CASE_INSENSITIVE; attributes.SecurityDescriptor = security_descriptor;
    IO_STATUS_BLOCK io{}; HANDLE raw = nullptr;
    // FILE_DIRECTORY_FILE combined with FILE_OPEN_NO_RECALL is rejected by NT.
    // Keep no-recall/reparse protection and verify directory type on this same
    // retained handle before enumeration or child traversal, as resource ingress does.
    const auto status = NtCreateFile(&raw, access | READ_CONTROL | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
        &attributes, &io, nullptr, FILE_ATTRIBUTE_NORMAL, share, disposition,
        0x00200000 | 0x00400000 | 0x00000020 | (directory ? 0UL : 0x40UL), nullptr, 0);
    if(status < 0) fail("child-open", static_cast<DWORD>(status));
    Handle result(raw); identity(result.get(), directory);
    alignas(FILE_NAME_INFO) std::array<unsigned char, 65536> buffer{};
    check(GetFileInformationByHandleEx(result.get(), FileNameInfo, buffer.data(), static_cast<DWORD>(buffer.size())), "name");
    const auto* info = reinterpret_cast<const FILE_NAME_INFO*>(buffer.data());
    require(info->FileNameLength > 0 && info->FileNameLength <= buffer.size() - offsetof(FILE_NAME_INFO, FileName)
        && info->FileNameLength % sizeof(wchar_t) == 0, "name");
    std::wstring actual(info->FileName, info->FileNameLength / sizeof(wchar_t));
    require(actual.substr(actual.find_last_of(L'\\') + 1) == name, "name-alias");
    return result;
}
std::string read_bytes(HANDLE handle, std::size_t maximum)
{
    const auto info = resource_platform::detail::inspect(handle, false, 0);
    const auto length = resource_platform::detail::size(info);
    require(length <= maximum, "size");
    std::string bytes(static_cast<std::size_t>(length), '\0');
    LARGE_INTEGER start{}; check(SetFilePointerEx(handle, start, nullptr, FILE_BEGIN), "seek");
    for(std::size_t offset = 0; offset < bytes.size();) {
        DWORD count = 0;
        check(ReadFile(handle, bytes.data() + offset, static_cast<DWORD>(std::min<std::size_t>(65536, bytes.size() - offset)), &count, nullptr), "read");
        require(count > 0, "short-read"); offset += count;
    }
    char extra = 0; DWORD count = 0; check(ReadFile(handle, &extra, 1, &count, nullptr), "read-end");
    require(count == 0 && resource_platform::detail::size(
        resource_platform::detail::inspect(handle, false, 0)) == length, "changed-size");
    return bytes;
}
std::set<std::wstring> entries(HANDLE directory, std::size_t& total)
{
    std::set<std::wstring> names;
    alignas(FILE_ID_BOTH_DIR_INFO) std::array<unsigned char, 65536> buffer{};
    bool restart = true;
    for(;;) {
        const auto ok = GetFileInformationByHandleEx(directory,
            restart ? FileIdBothDirectoryRestartInfo : FileIdBothDirectoryInfo,
            buffer.data(), static_cast<DWORD>(buffer.size()));
        restart = false;
        if(!ok) {
            const auto error = GetLastError();
            if(error == ERROR_NO_MORE_FILES) return names;
            fail("enumeration", error);
        }
        std::size_t offset = 0;
        for(;;) {
            require(offset <= buffer.size() - offsetof(FILE_ID_BOTH_DIR_INFO, FileName), "enumeration-shape");
            const auto* entry = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(buffer.data() + offset);
            require(entry->FileNameLength > 0 && entry->FileNameLength % sizeof(wchar_t) == 0
                && entry->FileNameLength <= buffer.size() - offset - offsetof(FILE_ID_BOTH_DIR_INFO, FileName), "enumeration-shape");
            const std::wstring name(entry->FileName, entry->FileNameLength / sizeof(wchar_t));
            if(name != L"." && name != L"..") {
                require(total < 128, "namespace-budget"); ++total;
                require(names.insert(name).second, "duplicate-entry");
            }
            if(entry->NextEntryOffset == 0) break;
            require(entry->NextEntryOffset >= offsetof(FILE_ID_BOTH_DIR_INFO, FileName) + entry->FileNameLength
                && entry->NextEntryOffset % alignof(FILE_ID_BOTH_DIR_INFO) == 0
                && entry->NextEntryOffset <= buffer.size() - offset, "enumeration-shape");
            offset += entry->NextEntryOffset;
        }
    }
}
struct Manifest {
    ManagedChainScope scope;
    ManagedIdentity root, directory, lock;
    std::string writer, client;
};
Manifest manifest(std::string_view bytes)
{
    std::size_t offset = 0;
    const auto take = [&](std::size_t length) {
        require(length <= bytes.size() - offset, "manifest-size");
        const auto result = bytes.substr(offset, length); offset += length; return result;
    };
    const auto integer = [&](unsigned count) {
        const auto data = take(count); std::uint64_t result = 0;
        for(unsigned i = 0; i < count; ++i) result |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[i])) << (8 * i);
        return result;
    };
    const auto id = [&]() { ManagedId result{}; const auto data = take(16); std::copy(data.begin(), data.end(), result.begin()); return result; };
    const auto file = [&]() { const auto serial = integer(8); return ManagedIdentity{serial, id()}; };
    require(take(8) == "SNST0001", "manifest-format");
    Manifest result; result.scope.generation = id(); result.scope.document = id();
    result.root = file(); result.directory = file(); result.lock = file(); result.scope.volume = result.root.volume;
    const auto writer_length = integer(4), client_length = integer(4);
    require(writer_length <= 68 && client_length <= 68, "manifest-size");
    result.writer = take(static_cast<std::size_t>(writer_length)); result.client = take(static_cast<std::size_t>(client_length));
    require(offset == bytes.size() && result.writer != result.client, "manifest-shape");
    binary_sid_text(result.writer); binary_sid_text(result.client);
    return result;
}
bool staging(const std::wstring& name)
{
    return name.size() == 42 && name.starts_with(L"stage-") && name.ends_with(L".tmp")
        && std::all_of(name.begin() + 6, name.begin() + 38, [](wchar_t c) { return c < 128 && hex(static_cast<char>(c)); });
}
bool committed(const std::wstring& name)
{
    return name.size() == 15 && name.ends_with(L".commit")
        && std::all_of(name.begin(), name.begin() + 8, [](wchar_t c) { return c < 128 && hex(static_cast<char>(c)); });
}
std::string ascii(const std::wstring& name)
{
    std::string result; for(const auto c : name) { require(c > 0 && c < 128, "name"); result += static_cast<char>(c); } return result;
}
std::wstring temporary_name()
{
    const auto random = random_id(); std::wstring result = L"stage-";
    for(const auto b : random) { result += L"0123456789abcdef"[b >> 4]; result += L"0123456789abcdef"[b & 15]; }
    return result + L".tmp";
}
}

struct ManagedStore::Impl {
    struct Object { std::wstring name; ManagedIdentity identity; Handle handle; bool orphan; std::string bytes; };
    std::string writer_sid, manifest_bytes;
    std::wstring writer;
    Manifest setup;
    ManagedId run_id{};
    Handle disk, root, directory, lock, manifest_file;
    std::vector<Object> objects;
    std::shared_ptr<const ManagedChain> chain;
    std::mutex mutex;
    bool usable = true;
    std::size_t entry_count = 0;

    explicit Impl(const std::string& root_leaf)
        : writer_sid(process_sid()), writer(binary_sid_text(writer_sid)), disk(volume()),
          root(child(disk.get(), wide(root_leaf), true, FILE_LIST_DIRECTORY, FILE_SHARE_READ)),
          // NT rename opens the target directory for addition internally. Permit
          // write sharing there; exact ACLs exclude clients and the immutable
          // no-share lock serializes trusted writers. Delete sharing stays off.
          directory(child(root.get(), L"document", true, FILE_LIST_DIRECTORY | FILE_ADD_FILE, FILE_SHARE_READ | FILE_SHARE_WRITE)),
          lock(child(directory.get(), L"writer.lock", false, FILE_READ_DATA, 0)),
          manifest_file(child(root.get(), L"generation.manifest", false, FILE_READ_DATA, FILE_SHARE_READ))
    {
        security(root.get(), writer, false, root_rights);
        security(directory.get(), writer, false, directory_rights);
        security(lock.get(), writer, false, read_rights);
        security(manifest_file.get(), writer, false, read_rights);
        require(read_bytes(lock.get(), 0).empty(), "lock-size");
        manifest_bytes = read_bytes(manifest_file.get(), 4096); setup = manifest(manifest_bytes);
        require(setup.writer == writer_sid && identity(root.get(), true) == setup.root
            && identity(directory.get(), true) == setup.directory && identity(lock.get(), false) == setup.lock
            && setup.directory.volume == setup.scope.volume && setup.lock.volume == setup.scope.volume, "provision-identity");
        require(entries(root.get(), entry_count) == std::set<std::wstring>{L"document", L"generation.manifest"}, "root-entries");
        const auto names = entries(directory.get(), entry_count);
        require(names.contains(L"writer.lock"), "lock-missing");
        std::vector<ManagedObservedRecord> observations;
        std::size_t scanned_bytes = 0;
        objects.reserve(127);
        for(const auto& name : names) {
            if(name == L"writer.lock") continue;
            const bool orphan = staging(name);
            require(orphan || committed(name), "unknown-entry");
            auto file = child(directory.get(), name, false, FILE_READ_DATA, FILE_SHARE_READ);
            security(file.get(), writer, true, record_rights);
            const auto file_identity = identity(file.get(), false);
            require(file_identity.volume == setup.scope.volume, "file-volume");
            const auto length = resource_platform::detail::size(resource_platform::detail::inspect(file.get(), false, 0));
            require(length <= managed_record_max_bytes, "file-size");
            std::string bytes;
            if(!orphan) {
                require(observations.size() < managed_revision_limit && length <= 128 * 1024 * 1024 - scanned_bytes, "scan-budget");
                scanned_bytes += static_cast<std::size_t>(length);
                bytes = read_bytes(file.get(), managed_record_max_bytes);
                observations.push_back({ascii(name), bytes, file_identity});
            }
            objects.push_back({name, file_identity, std::move(file), orphan, std::move(bytes)});
        }
        const auto validated = validate_managed_chain(setup.scope, observations);
        require(std::holds_alternative<std::shared_ptr<const ManagedChain>>(validated), "chain");
        chain = std::get<std::shared_ptr<const ManagedChain>>(validated);
        run_id = random_id(); audit(); boundary("opened");
    }
    void audit()
    {
        require(usable, "unavailable");
        volume_security(disk.get());
        security(root.get(), writer, false, root_rights); security(directory.get(), writer, false, directory_rights);
        security(lock.get(), writer, false, read_rights); security(manifest_file.get(), writer, false, read_rights);
        require(identity(root.get(), true) == setup.root && identity(directory.get(), true) == setup.directory
            && identity(lock.get(), false) == setup.lock && read_bytes(manifest_file.get(), 4096) == manifest_bytes, "setup-changed");
        std::size_t count = 0;
        require(entries(root.get(), count) == std::set<std::wstring>{L"document", L"generation.manifest"}, "root-entries");
        const auto actual = entries(directory.get(), count);
        std::set<std::wstring> expected{L"writer.lock"};
        for(auto& object : objects) {
            require(expected.insert(object.name).second, "duplicate-entry");
            security(object.handle.get(), writer, true, record_rights);
            require(identity(object.handle.get(), false) == object.identity, "object-changed");
            if(!object.orphan) require(read_bytes(object.handle.get(), managed_record_max_bytes) == object.bytes, "bytes-changed");
            else require(resource_platform::detail::size(resource_platform::detail::inspect(object.handle.get(), false, 0))
                <= managed_record_max_bytes, "orphan-size");
        }
        require(actual == expected, "namespace-changed"); entry_count = count;
    }
};

ManagedStore::ManagedStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ManagedStore::~ManagedStore() = default;
ManagedId ManagedStore::run() const noexcept { return impl_->run_id; }
std::string ManagedStore::allowed_principal() const { return impl_->setup.client; }
ManagedStoreOpen open_managed_store(const std::string& root_leaf)
{
    try {
        require(root_name(root_leaf), "root-name");
        auto impl = std::make_unique<ManagedStore::Impl>(root_leaf);
        return std::unique_ptr<ManagedStore>(new ManagedStore(std::move(impl)));
    } catch(ManagedStoreError error) { return error; }
    catch(const ResourceError& error) { return ManagedStoreError{"physical", error.system_code}; }
    catch(const std::bad_alloc&) { return ManagedStoreError{"memory"}; }
}
ManagedStoreRead ManagedStore::read()
{
    std::unique_lock guard(impl_->mutex, std::try_to_lock);
    if(!guard.owns_lock()) return ManagedStoreError{"busy"};
    try { impl_->audit(); return impl_->chain; }
    catch(ManagedStoreError error) { impl_->usable = false; return error; }
    catch(const ResourceError& error) { impl_->usable = false; return ManagedStoreError{"physical", error.system_code}; }
    catch(const std::bad_alloc&) { impl_->usable = false; return ManagedStoreError{"memory"}; }
}
std::variant<ManagedReceiptLookup, ManagedStoreError> ManagedStore::reconcile(
    const ManagedId& run, const ManagedCommitRequest& request)
{
    std::unique_lock guard(impl_->mutex, std::try_to_lock);
    if(!guard.owns_lock()) return ManagedStoreError{"busy"};
    try {
        require(run == impl_->run_id, "run"); require(request.principal_sid == impl_->setup.client, "principal");
        impl_->audit(); return lookup_managed_receipt(*impl_->chain, request);
    } catch(ManagedStoreError error) { return error; }
    catch(const ResourceError& error) { impl_->usable = false; return ManagedStoreError{"physical", error.system_code}; }
    catch(const std::bad_alloc&) { return ManagedStoreError{"memory"}; }
}
ManagedStoreSave ManagedStore::save(const ManagedId& run, const ManagedCommitRequest& request)
{
    std::unique_lock guard(impl_->mutex, std::try_to_lock);
    if(!guard.owns_lock()) return ManagedStoreError{"busy"};
    bool created = false, published = false;
    try {
        require(run == impl_->run_id, "run"); require(request.principal_sid == impl_->setup.client, "principal");
        impl_->audit();
        const auto plan = prepare_managed_commit(*impl_->chain, request, random_id());
        if(const auto* receipt = std::get_if<ManagedReceipt>(&plan)) return *receipt;
        if(const auto* error = std::get_if<ManagedChainError>(&plan)) return ManagedStoreError{"request", static_cast<DWORD>(*error)};
        require(impl_->entry_count < 128, "namespace-budget");
        const auto& prepared = std::get<ManagedPreparedCommit>(plan);
        const auto sddl = L"O:" + impl_->writer + L"D:P(A;;0x1f01ff;;;SY)(A;;0x1f01ff;;;BA)(A;;0x13019f;;;" + impl_->writer + L")";
        PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
        check(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &raw_descriptor, nullptr), "create-security");
        LocalMemory descriptor{raw_descriptor};
        std::optional<Handle> temporary;
        boundary("before-stage");
        for(unsigned attempt = 0; attempt < 8; ++attempt) {
            try {
                temporary.emplace(child(impl_->directory.get(), temporary_name(), false,
                    GENERIC_READ | GENERIC_WRITE | DELETE, 0, FILE_CREATE, raw_descriptor));
                created = true; break;
            } catch(const ManagedStoreError& error) {
                if(error.system_code != 0xc0000035UL) throw;
            }
        }
        require(temporary.has_value(), "staging-collisions");
        const auto file_identity = identity(temporary->get(), false);
        security(temporary->get(), impl_->writer, true, record_rights);
        require(file_identity.volume == impl_->setup.scope.volume, "file-volume");
        boundary("created");
        for(std::size_t offset = 0; offset < prepared.bytes.size();) {
            const auto amount = static_cast<DWORD>(std::min<std::size_t>(65536, prepared.bytes.size() - offset));
            DWORD count = 0; check(WriteFile(temporary->get(), prepared.bytes.data() + offset, amount, &count, nullptr), "write");
            require(count > 0 && count <= amount, "short-write"); offset += count; boundary("chunk");
        }
        boundary("written"); boundary("flush"); check(FlushFileBuffers(temporary->get()), "flush"); boundary("flushed");
        require(read_bytes(temporary->get(), managed_record_max_bytes) == prepared.bytes, "verification");
        security(temporary->get(), impl_->writer, true, record_rights); boundary("verified");
        // Allocate the replacement in-memory snapshot and receipt before publication.
        // The original chain remains untouched if any prepublication step fails.
        std::vector<ManagedObservedRecord> next;
        next.reserve(impl_->chain->entries().size() + 1);
        for(const auto& object : impl_->objects)
            if(!object.orphan) next.push_back({ascii(object.name), object.bytes, object.identity});
        next.push_back({prepared.leaf, prepared.bytes, file_identity});
        auto validated = validate_managed_chain(impl_->setup.scope, next);
        require(std::holds_alternative<std::shared_ptr<const ManagedChain>>(validated), "prepared-chain");
        auto future = std::get<std::shared_ptr<const ManagedChain>>(std::move(validated));
        const ManagedReceipt receipt{request.operation, future->current(), prepared.request_digest};
        Impl::Object object{wide(prepared.leaf), file_identity, std::move(*temporary), false, prepared.bytes};
        alignas(FILE_RENAME_INFO) std::array<unsigned char, sizeof(FILE_RENAME_INFO) + 30> storage{};
        auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
        rename->ReplaceIfExists = FALSE; rename->RootDirectory = impl_->directory.get();
        rename->FileNameLength = static_cast<DWORD>(object.name.size() * sizeof(wchar_t));
        std::memcpy(rename->FileName, object.name.data(), rename->FileNameLength);
        boundary("before-publish");
        IO_STATUS_BLOCK io{};
        const auto status = NtSetInformationFile(object.handle.get(), &io, rename,
            static_cast<ULONG>(storage.size()), static_cast<FILE_INFORMATION_CLASS>(10));
        if(status < 0) fail("publish", static_cast<DWORD>(status));
        published = true; boundary("published");
        require(identity(object.handle.get(), false) == file_identity
            && read_bytes(object.handle.get(), managed_record_max_bytes) == prepared.bytes, "published-verification");
        security(object.handle.get(), impl_->writer, true, record_rights);
        impl_->objects.push_back(std::move(object)); impl_->chain = std::move(future);
        impl_->audit(); boundary("receipt");
        return receipt;
    } catch(ManagedStoreError error) {
        if(created || published) impl_->usable = false;
        error.indeterminate = published; return error;
    } catch(const ResourceError& error) {
        impl_->usable = false; return ManagedStoreError{"physical", error.system_code, published};
    } catch(const std::bad_alloc&) {
        if(created || published) impl_->usable = false;
        return ManagedStoreError{"memory", 0, published};
    }
}
}
