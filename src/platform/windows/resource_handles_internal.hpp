// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/local_resources.hpp"
#include <windows.h>
#include <winternl.h>
#include <algorithm>
#include <array>
#include <tuple>
#include <utility>

namespace simnodus::resource_platform::detail {
class Handle {
public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { if(value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(Handle&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_;
};

[[noreturn]] inline void fail(ResourceErrorCode code, std::size_t index, DWORD system = 0)
{
    throw ResourceError{code, index, system};
}
inline void check(BOOL result, std::size_t index)
{
    if(!result) fail(ResourceErrorCode::filesystem, index, GetLastError());
}
using Identity = std::tuple<DWORD, DWORD, DWORD>;
inline Identity identity(const BY_HANDLE_FILE_INFORMATION& value)
{
    return {value.dwVolumeSerialNumber, value.nFileIndexHigh, value.nFileIndexLow};
}
inline std::uint64_t size(const BY_HANDLE_FILE_INFORMATION& value)
{
    return (static_cast<std::uint64_t>(value.nFileSizeHigh) << 32) | value.nFileSizeLow;
}
inline BY_HANDLE_FILE_INFORMATION inspect(HANDLE handle, bool directory, std::size_t index)
{
    BY_HANDLE_FILE_INFORMATION info{};
    check(GetFileInformationByHandle(handle, &info), index);
    if(GetFileType(handle) != FILE_TYPE_DISK) fail(ResourceErrorCode::type, index);
    if(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_OFFLINE))
        fail(ResourceErrorCode::reparse, index);
    if(((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory)
        fail(ResourceErrorCode::type, index);
    if(directory) {
        FILE_CASE_SENSITIVE_INFO policy{};
        if(!GetFileInformationByHandleEx(handle, FileCaseSensitiveInfo, &policy, sizeof(policy)))
            fail(ResourceErrorCode::case_sensitive, index, GetLastError());
        if(policy.Flags != 0) fail(ResourceErrorCode::case_sensitive, index);
    } else if(info.nNumberOfLinks != 1) fail(ResourceErrorCode::alias, index);
    return info;
}

inline Handle child(HANDLE parent, const std::wstring& name, bool directory, std::size_t index)
{
    UNICODE_STRING text{};
    text.Buffer = const_cast<wchar_t*>(name.data());
    text.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
    text.MaximumLength = text.Length;
    OBJECT_ATTRIBUTES attributes{};
    attributes.Length = sizeof(attributes);
    attributes.RootDirectory = parent;
    attributes.ObjectName = &text;
    attributes.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK status{};
    HANDLE raw = nullptr;
    // Open the single component itself, with synchronous reads and no recall.
    constexpr ULONG options = 0x00200000 | 0x00400000 | 0x00000020;
    const auto result = NtCreateFile(&raw, SYNCHRONIZE | FILE_READ_ATTRIBUTES
        | (directory ? 0UL : FILE_READ_DATA), &attributes, &status, nullptr, 0,
        FILE_SHARE_READ, FILE_OPEN, options, nullptr, 0);
    if(result < 0) fail(ResourceErrorCode::filesystem, index, static_cast<DWORD>(result));
    Handle handle(raw);
    inspect(handle.get(), directory, index);
    alignas(FILE_NAME_INFO) std::array<unsigned char, 65536> storage{};
    check(GetFileInformationByHandleEx(handle.get(), FileNameInfo, storage.data(),
        static_cast<DWORD>(storage.size())), index);
    const auto* info = reinterpret_cast<const FILE_NAME_INFO*>(storage.data());
    if(info->FileNameLength == 0 || info->FileNameLength > storage.size() - offsetof(FILE_NAME_INFO, FileName)
        || info->FileNameLength % sizeof(wchar_t) != 0) fail(ResourceErrorCode::alias, index);
    std::wstring actual(info->FileName, info->FileNameLength / sizeof(wchar_t));
    actual = actual.substr(actual.find_last_of(L'\\') + 1);
    if(CompareStringOrdinal(actual.data(), static_cast<int>(actual.size()), name.data(),
        static_cast<int>(name.size()), TRUE) != CSTR_EQUAL) fail(ResourceErrorCode::alias, index);
    return handle;
}

inline std::wstring wide_root(const std::string& root)
{
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, root.data(),
        static_cast<int>(root.size()), nullptr, 0);
    if(length <= 0 || length > 1024) fail(ResourceErrorCode::root, resource_global_error);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, root.data(), static_cast<int>(root.size()),
        result.data(), length) != length) fail(ResourceErrorCode::root, resource_global_error);
    std::replace(result.begin(), result.end(), L'/', L'\\');
    return result;
}

inline Handle volume(const std::wstring& root)
{
    const auto drive = root.substr(0, 2);
    if(GetDriveTypeW((drive + L"\\").c_str()) != DRIVE_FIXED)
        fail(ResourceErrorCode::platform, resource_global_error);
    std::array<wchar_t, 1024> target{};
    check(QueryDosDeviceW(drive.c_str(), target.data(), static_cast<DWORD>(target.size())) != 0,
        resource_global_error);
    const std::wstring device(target.data());
    const std::wstring prefix = L"\\Device\\HarddiskVolume";
    if(!device.starts_with(prefix) || device.size() == prefix.size()
        || !std::all_of(device.begin() + static_cast<std::ptrdiff_t>(prefix.size()), device.end(),
            [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
        fail(ResourceErrorCode::platform, resource_global_error);
    Handle handle(CreateFileW((L"\\\\?\\GLOBALROOT" + device + L"\\").c_str(),
        SYNCHRONIZE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OPEN_NO_RECALL, nullptr));
    if(handle.get() == INVALID_HANDLE_VALUE)
        fail(ResourceErrorCode::filesystem, resource_global_error, GetLastError());
    inspect(handle.get(), true, resource_global_error);
    std::array<wchar_t, 32> filesystem{};
    check(GetVolumeInformationByHandleW(handle.get(), nullptr, 0, nullptr, nullptr, nullptr,
        filesystem.data(), static_cast<DWORD>(filesystem.size())), resource_global_error);
    if(std::wstring_view(filesystem.data()) != L"NTFS") fail(ResourceErrorCode::platform, resource_global_error);
    return handle;
}

}
