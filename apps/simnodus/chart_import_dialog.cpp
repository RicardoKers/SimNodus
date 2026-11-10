// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "chart_import_dialog.hpp"

#include "model/engineering.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace simnodus::ui {

ChartImportDialog::ChartImportDialog(const io::CsvTable& table, const QString& file_name, QWidget* parent)
    : QDialog(parent), table_(table)
{
    setWindowTitle(tr("Import Reference Data"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("%1: %n rows of numbers.", nullptr, static_cast<int>(table.rows.size())).arg(file_name));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* form = new QFormLayout;
    time_column_ = new QComboBox;
    value_column_ = new QComboBox;
    for (std::size_t i = 0; i < table.columns; ++i) {
        QString name = tr("Column %1").arg(i + 1);
        if (i < table.header.size() && !table.header[i].empty()) {
            name += QStringLiteral(" (%1)").arg(QString::fromStdString(table.header[i]));
        }
        time_column_->addItem(name);
        value_column_->addItem(name);
    }
    time_column_->setCurrentIndex(0);
    value_column_->setCurrentIndex(1);
    time_unit_ = new QComboBox;
    time_unit_->addItem(tr("seconds"), 1.0);
    time_unit_->addItem(tr("milliseconds"), 1e-3);
    time_unit_->addItem(tr("minutes"), 60.0);
    time_unit_->addItem(tr("hours"), 3600.0);
    time_offset_ = new QLineEdit(QStringLiteral("0"));
    time_offset_->setToolTip(tr("Seconds added to every time, so that the bench's start matches the run's start"));
    value_unit_ = new QComboBox;
    value_unit_->addItem(tr("volts"), 1.0);
    value_unit_->addItem(tr("millivolts"), 1e-3);
    form->addRow(tr("Time column"), time_column_);
    form->addRow(tr("Time unit"), time_unit_);
    form->addRow(tr("Time offset (s)"), time_offset_);
    form->addRow(tr("Value column"), value_column_);
    form->addRow(tr("Value unit"), value_unit_);
    layout->addLayout(form);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (auto* box : {time_column_, value_column_, time_unit_, value_unit_}) {
        connect(box, &QComboBox::currentIndexChanged, this, &ChartImportDialog::update_summary);
    }
    connect(time_offset_, &QLineEdit::textChanged, this, &ChartImportDialog::update_summary);
    update_summary();
}

io::ReferenceOptions ChartImportDialog::options() const
{
    io::ReferenceOptions o;
    o.time_column = static_cast<std::size_t>(std::max(0, time_column_->currentIndex()));
    o.value_column = static_cast<std::size_t>(std::max(0, value_column_->currentIndex()));
    o.time_scale = time_unit_->currentData().toDouble();
    o.time_offset = model::parse_engineering(time_offset_->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toStdString(), "s")
                        .value_or(0.0);
    o.value_scale = value_unit_->currentData().toDouble();
    return o;
}

void ChartImportDialog::update_summary()
{
    const auto s = series();
    if (s.empty()) {
        summary_->setText(tr("No rows have numbers in both columns."));
    } else {
        summary_->setText(tr("%n points from %1 s to %2 s, values from %3 V to %4 V.", nullptr, static_cast<int>(s.size()))
                              .arg(s.front().first, 0, 'g', 4)
                              .arg(s.back().first, 0, 'g', 4)
                              .arg(std::min_element(s.begin(), s.end(), [](auto& a, auto& b) { return a.second < b.second; })->second, 0, 'g', 4)
                              .arg(std::max_element(s.begin(), s.end(), [](auto& a, auto& b) { return a.second < b.second; })->second, 0, 'g', 4));
    }
    if (auto* box = findChild<QDialogButtonBox*>()) {
        box->button(QDialogButtonBox::Ok)->setEnabled(!s.empty());
    }
}

std::optional<io::Series> import_reference_csv(QWidget* parent, QString& error)
{
    const QString path = QFileDialog::getOpenFileName(parent, QObject::tr("Import Reference Data"), QString(),
        QObject::tr("Measurements (*.csv *.txt *.tsv);;All files (*)"));
    if (path.isEmpty()) {
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024) {
        error = QObject::tr("Could not read %1.").arg(path);
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    std::string problem;
    const auto table = io::parse_csv(bytes.toStdString(), problem);
    if (!table) {
        error = QObject::tr("%1: %2").arg(QFileInfo(path).fileName(), QString::fromStdString(problem));
        return std::nullopt;
    }
    ChartImportDialog dialog(*table, QFileInfo(path).fileName(), parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.series();
}

} // namespace simnodus::ui
