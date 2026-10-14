// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "platform/project_save_backend.hpp"
#include "platform/windows/resource_handles_internal.hpp"
#include <bcrypt.h>
#include <cstring>
#include <new>

// The installed user-mode SDK exposes only a subset of the native file API.
// Match the documented native signature; ntdll is a linked Windows system API.
extern "C" NTSYSAPI NTSTATUS NTAPI NtSetInformationFile(
    HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

namespace simnodus::save_platform {
namespace {
using namespace resource_platform::detail;
void checked(BOOL ok)
{
    if(!ok) throw ProjectSaveError{"filesystem", 0, GetLastError()};
}
void boundary(const char* phase)
{
#ifdef SIMNODUS_SAVE_TEST_HOOKS
    if(!test_boundary(phase)) throw ProjectSaveError{"injected"};
#else
    (void)phase;
#endif
}
class Temporary {
public:
    Temporary(HANDLE handle, bool& cleanup_failed) : handle_(handle), cleanup_failed_(cleanup_failed) {}
    ~Temporary()
    {
        if(!committed_) {
            FILE_DISPOSITION_INFO disposition{TRUE};
            if(!SetFileInformationByHandle(handle_.get(), FileDispositionInfo, &disposition, sizeof(disposition)))
                cleanup_failed_ = true;
        }
    }
    HANDLE get() const noexcept { return handle_.get(); }
    void committed() noexcept { committed_ = true; }
private:
    Handle handle_;
    bool& cleanup_failed_;
    bool committed_ = false;
};
HANDLE create_temporary(HANDLE parent)
{
    for(unsigned attempt = 0; attempt < 8; ++attempt) {
        std::array<unsigned char, 16> random{};
        const auto generated = BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if(generated < 0) throw ProjectSaveError{"filesystem", 0, static_cast<DWORD>(generated)};
        std::wstring name = L"sn-save-";
        for(const auto value : random) { name += L"0123456789abcdef"[value >> 4]; name += L"0123456789abcdef"[value & 15]; }
        name += L".tmp";
        UNICODE_STRING text{};
        text.Buffer = name.data();
        text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
        text.MaximumLength = text.Length;
        OBJECT_ATTRIBUTES attributes{};
        attributes.Length = sizeof(attributes);
        attributes.RootDirectory = parent;
        attributes.ObjectName = &text;
        attributes.Attributes = OBJ_CASE_INSENSITIVE;
        IO_STATUS_BLOCK io{};
        HANDLE raw = nullptr;
        // FILE_CREATE never opens/truncates an existing entry; open the entry
        // itself without reparse traversal, recall or directory interpretation.
        constexpr ULONG options = 0x00200000 | 0x00400000 | 0x00000020 | 0x00000040;
        const auto status = NtCreateFile(&raw, GENERIC_WRITE | DELETE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
            &attributes, &io, nullptr, FILE_ATTRIBUTE_NORMAL, 0, FILE_CREATE, options, nullptr, 0);
        if(status >= 0) return raw;
        if(static_cast<ULONG>(status) != 0xc0000035UL)
            throw ProjectSaveError{"filesystem", 0, static_cast<DWORD>(status)};
    }
    throw ProjectSaveError{"temporary-collision"};
}
}
ProjectSaveResult create(const std::string& root, const std::string& filename, const std::string& bytes)
{
    using namespace resource_platform::detail;
    bool cleanup_failed = false;
    try {
        const auto wide = wide_root(root);
        std::vector<Handle> ancestors;
        ancestors.emplace_back(volume(wide));
        HANDLE parent = ancestors.back().get();
        for(std::size_t start = 3; start < wide.size();) {
            auto end = wide.find(L'\\', start);
            if(end == wide.npos) end = wide.size();
            ancestors.emplace_back(child(parent, wide.substr(start, end - start), true, resource_global_error));
            parent = ancestors.back().get();
            start = end + 1;
        }
        boundary("root");
        Temporary temporary(create_temporary(parent), cleanup_failed);
        inspect(temporary.get(), false, resource_global_error);
        boundary("created");
        std::size_t offset = 0;
        while(offset < bytes.size()) {
            const auto amount = static_cast<DWORD>(std::min<std::size_t>(65536, bytes.size() - offset));
            DWORD written = 0;
            checked(WriteFile(temporary.get(), bytes.data() + offset, amount, &written, nullptr));
            if(written == 0) throw ProjectSaveError{"write"};
            offset += written;
            boundary("chunk");
        }
        boundary("written");
        checked(FlushFileBuffers(temporary.get()));
        boundary("flushed");
        if(size(inspect(temporary.get(), false, resource_global_error)) != bytes.size()) throw ProjectSaveError{"size"};
        // Allocate all fallible state before the single publication operation.
        // Use the already-open parent and one bounded leaf; never resolve/reopen.
        const std::wstring name(filename.begin(), filename.end());
        alignas(FILE_RENAME_INFO) std::array<unsigned char, sizeof(FILE_RENAME_INFO) + 81 * sizeof(wchar_t)> storage{};
        auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
        rename->ReplaceIfExists = FALSE;
        rename->RootDirectory = parent;
        rename->FileNameLength = static_cast<DWORD>(name.size() * sizeof(wchar_t));
        std::memcpy(rename->FileName, name.data(), rename->FileNameLength);
        boundary("publish");
        IO_STATUS_BLOCK io{};
        const auto status = NtSetInformationFile(temporary.get(), &io, rename,
            static_cast<ULONG>(storage.size()), static_cast<FILE_INFORMATION_CLASS>(10));
        if(status < 0) {
            const auto error = static_cast<ULONG>(status);
            throw ProjectSaveError{error == 0xc0000035UL ? "exists" : "filesystem", 0, error};
        }
        temporary.committed();
        return ProjectSaveReceipt{bytes.size()};
    } catch(ProjectSaveError error) {
        error.temporary_cleanup_failed = cleanup_failed;
        return error;
    } catch(const ResourceError& error) {
        return ProjectSaveError{resource_error_name(error.code), 0, error.system_code, cleanup_failed};
    } catch(const std::bad_alloc&) {
        return ProjectSaveError{"memory", 0, 0, cleanup_failed};
    }
}
}
