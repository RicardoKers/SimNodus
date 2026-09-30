#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// Owned, inert test data. This is not a project format or instrumentation API.
namespace sn022 {
inline constexpr std::string_view document =
    "{\"fixture\":\"SN-022\",\"editable_name\":\"Unsaved classroom circuit\"}\n";
inline constexpr std::string_view complete_record =
    "SN022/1\nsnapshot=41\nvoltage_uv=1234567\nEND\n";
inline constexpr std::string_view partial_record =
    "SN022/1\nsnapshot=99\nvoltage_uv=";
inline constexpr std::size_t output_limit = 128;

struct Snapshot {
    std::uint64_t sequence = 41;
    std::int64_t voltage_uv = 1234567;
    bool operator==(const Snapshot&) const = default;
};
} // namespace sn022
