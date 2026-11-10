// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "io/reference_csv.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace simnodus::io {
namespace {

std::string trim(std::string s)
{
    const auto not_space = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

std::optional<double> number(std::string field, bool decimal_comma)
{
    field = trim(std::move(field));
    if (field.empty()) {
        return std::nullopt;
    }
    if (decimal_comma) {
        std::replace(field.begin(), field.end(), ',', '.');
    }
    if (field.front() == '+') {
        field.erase(field.begin());
    }
    double value = 0.0;
    const char* begin = field.data();
    const char* end = begin + field.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc() || ptr != end || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::string> split(const std::string& line, char separator)
{
    std::vector<std::string> fields;
    if (separator == ' ') {
        std::string field;
        for (char c : line) {
            if (c == ' ' || c == '\t') {
                if (!field.empty()) {
                    fields.push_back(field);
                    field.clear();
                }
            } else {
                field += c;
            }
        }
        if (!field.empty()) {
            fields.push_back(field);
        }
        return fields;
    }
    std::string field;
    bool quoted = false;
    for (char c : line) {
        if (c == '"') {
            quoted = !quoted;
            field += c;
        } else if (c == separator && !quoted) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);
    return fields;
}

} // namespace

std::optional<CsvTable> parse_csv(const std::string& source, std::string& error)
{
    std::string text = source;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3); // UTF-8 byte order mark
    }
    CsvTable table;
    table.separator = text.find(';') != std::string::npos ? ';'
        : text.find('\t') != std::string::npos           ? '\t'
        : text.find(',') != std::string::npos            ? ','
                                                         : ' ';
    const bool decimal_comma = table.separator == ';' || table.separator == '\t';
    std::size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = trim(text.substr(start, end - start));
        start = end + 1;
        if (line.empty() || line.front() == '#') {
            if (!line.empty()) {
                ++table.skipped;
            }
            if (end == text.size()) {
                break;
            }
            continue;
        }
        const auto fields = split(line, table.separator);
        std::vector<double> row;
        bool any = false;
        for (const auto& f : fields) {
            const auto v = number(f, decimal_comma);
            any = any || v.has_value();
            row.push_back(v.value_or(std::numeric_limits<double>::quiet_NaN()));
        }
        if (!any) {
            ++table.skipped;
            if (table.header.empty() && table.rows.empty()) {
                for (const auto& f : fields) {
                    table.header.push_back(trim(f));
                }
            }
        } else {
            table.columns = std::max(table.columns, row.size());
            table.rows.push_back(std::move(row));
        }
        if (end == text.size()) {
            break;
        }
    }
    if (table.rows.empty()) {
        error = "the file has no rows of numbers";
        return std::nullopt;
    }
    if (table.columns < 2) {
        error = "the file needs two columns: time and value";
        return std::nullopt;
    }
    return table;
}

Series reference_series(const CsvTable& table, const ReferenceOptions& options)
{
    Series series;
    for (const auto& row : table.rows) {
        if (options.time_column >= row.size() || options.value_column >= row.size()) {
            continue;
        }
        const double t = row[options.time_column];
        const double v = row[options.value_column];
        if (std::isnan(t) || std::isnan(v)) {
            continue;
        }
        series.emplace_back(t * options.time_scale + options.time_offset, v * options.value_scale);
    }
    std::stable_sort(series.begin(), series.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const std::size_t limit = std::max<std::size_t>(options.max_points, 2);
    if (series.size() > limit) {
        const std::size_t step = (series.size() + limit - 1) / limit;
        Series thinned;
        for (std::size_t i = 0; i < series.size(); i += step) {
            thinned.push_back(series[i]);
        }
        if (thinned.back() != series.back()) {
            thinned.push_back(series.back());
        }
        series = std::move(thinned);
    }
    return series;
}

std::string encode_series(const Series& series)
{
    std::string out;
    char buffer[64];
    for (const auto& [t, v] : series) {
        if (!out.empty()) {
            out += ';';
        }
        std::snprintf(buffer, sizeof buffer, "%.6g %.6g", t, v);
        out += buffer;
    }
    return out;
}

Series decode_series(const std::string& text)
{
    Series series;
    for (const auto& pair : split(text, ';')) {
        const auto fields = split(pair, ' ');
        if (fields.size() != 2) {
            continue;
        }
        const auto t = number(fields[0], false);
        const auto v = number(fields[1], false);
        if (t && v) {
            series.emplace_back(*t, *v);
        }
    }
    return series;
}

} // namespace simnodus::io
