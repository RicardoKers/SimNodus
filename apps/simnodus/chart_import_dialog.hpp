// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Imports reference data of a chart recorder channel (SN-281, AN-11) from a
// CSV file of bench measurements: the columns of time and value, the time
// unit and offset, and the value unit.
#pragma once

#ifndef Q_MOC_RUN
#include "io/reference_csv.hpp"
#endif

#include <QDialog>
#include <QString>

#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;

namespace simnodus::ui {

class ChartImportDialog : public QDialog {
    Q_OBJECT
public:
    ChartImportDialog(const io::CsvTable& table, const QString& file_name, QWidget* parent = nullptr);

    io::ReferenceOptions options() const;
    io::Series series() const { return io::reference_series(table_, options()); }

private:
    void update_summary();

    io::CsvTable table_;
    QComboBox* time_column_ = nullptr;
    QComboBox* value_column_ = nullptr;
    QComboBox* time_unit_ = nullptr;
    QLineEdit* time_offset_ = nullptr;
    QComboBox* value_unit_ = nullptr;
    QLabel* summary_ = nullptr;
};

// Asks for a CSV file and its columns; the series to keep, or nothing when
// cancelled (or the file could not be read: `error` says why).
std::optional<io::Series> import_reference_csv(QWidget* parent, QString& error);

} // namespace simnodus::ui
