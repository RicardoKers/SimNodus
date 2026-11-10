// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Reference data of a chart recorder (SN-281, AN-11): a table of bench
// measurements read from CSV text, and the compact text that keeps the chosen
// series in the project (a part's parameter), so that opening a project
// never reads another file.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::io {

struct CsvTable {
    std::vector<std::string> header;       // names of the columns, from the first line that is not numbers; may be empty
    std::vector<std::vector<double>> rows; // numeric rows (a field that is not a number is NaN)
    std::size_t columns = 0;               // the most fields of a numeric row
    std::size_t skipped = 0;               // lines that were not data (header, comments, empty)
    char separator = ',';                  // ',', ';', '\t' or ' ' (spaces)
};

// Parses CSV text: fields separated by ';', a tab, ',' or spaces (the first
// of these the text has); with ';' or a tab a decimal comma is accepted.
// Lines without a number are skipped (a header names the columns). Nothing
// on failure, with `error`.
std::optional<CsvTable> parse_csv(const std::string& text, std::string& error);

struct ReferenceOptions {
    std::size_t time_column = 0;  // zero-based
    std::size_t value_column = 1; // zero-based
    double time_scale = 1.0;      // seconds per unit of the time column (60: minutes, 1e-3: milliseconds)
    double time_offset = 0.0;     // seconds added after the scale (the bench's start on the simulation's time)
    double value_scale = 1.0;     // volts per unit of the value column (1e-3: millivolts)
    std::size_t max_points = 20000;
};

using Series = std::vector<std::pair<double, double>>; // seconds, value; increasing time

// The series of two columns of a table: rows with both numbers, sorted by
// time; thinned to `max_points` (every n-th row, the last row kept).
Series reference_series(const CsvTable& table, const ReferenceOptions& options);

// The compact text kept in a project: "t v;t v;..." with six significant digits.
std::string encode_series(const Series& series);
// Parses it back; pairs that are not two numbers are left out.
Series decode_series(const std::string& text);

} // namespace simnodus::io
