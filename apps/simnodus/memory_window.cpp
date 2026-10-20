// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "memory_window.hpp"

#include "document.hpp"
#include "run_controller.hpp"

#include "editing/operations.hpp"
#include "io/memory_file.hpp"
#include "library/component_library.hpp"

#include <QComboBox>
#include <QSignalBlocker>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace simnodus::ui {

namespace {

constexpr int columns = 16;
constexpr qint64 highlight_ms = 1500;

} // namespace

MemoryWindow::MemoryWindow(Document& document, RunController& run, QWidget* parent)
    : QWidget(parent, Qt::Window), document_(document), run_(run)
{
    setObjectName(QStringLiteral("memoryWindow"));
    setWindowTitle(tr("Memory Viewer - SimNodus"));
    setAttribute(Qt::WA_QuitOnClose, false);
    resize(720, 520);

    chips_ = new QComboBox;
    chips_->setObjectName(QStringLiteral("memoryChips"));
    auto* load = new QPushButton(tr("Load..."));
    load->setObjectName(QStringLiteral("memoryLoad"));
    load->setToolTip(tr("Put a .bin or Intel .hex file into the running chip; when nothing runs, make it the chip's "
                        "contents file"));
    auto* save = new QPushButton(tr("Save as..."));
    save->setObjectName(QStringLiteral("memorySave"));
    save->setToolTip(tr("Write what the chip holds to a .bin or Intel .hex file"));
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Memory:")));
    top->addWidget(chips_, 1);
    top->addWidget(load);
    top->addWidget(save);

    table_ = new QTableWidget(0, columns + 1);
    table_->setObjectName(QStringLiteral("memoryTable"));
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    QStringList headers;
    for (int c = 0; c < columns; ++c) {
        headers << QStringLiteral("%1").arg(c, 1, 16).toUpper();
    }
    headers << tr("Text");
    table_->setHorizontalHeaderLabels(headers);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setFont(table_->font());
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(QFontMetrics(table_->font()).height() + 6);

    status_ = new QLabel;
    status_->setObjectName(QStringLiteral("memoryStatus"));
    status_->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(table_, 1);
    layout->addWidget(status_);

    connect(chips_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(load, &QPushButton::clicked, this, &MemoryWindow::load);
    connect(save, &QPushButton::clicked, this, &MemoryWindow::save);
    connect(&document_, &Document::changed, this, [this] {
        if (isVisible()) {
            rebuild_list();
        }
    });
    timer_.setInterval(250);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (isVisible()) {
            refresh();
        }
    });
    timer_.start();
}

void MemoryWindow::show_chip(model::Id component)
{
    rebuild_list();
    if (component != model::no_id) {
        const int index = chips_->findData(QVariant::fromValue<qulonglong>(component));
        if (index >= 0) {
            chips_->setCurrentIndex(index);
        }
    }
    refresh();
    show();
    raise();
    activateWindow();
}

void MemoryWindow::rebuild_list()
{
    const model::Id keep = current();
    QSignalBlocker block(chips_);
    chips_->clear();
    const auto& library = document_.library();
    for (const auto& c : document_.schematic().components()) {
        const auto* d = library.find(c.type);
        if (d && d->device && d->device->memory_bytes(c.parameters) > 0) {
            chips_->addItem(QStringLiteral("%1 (%2, %3 bytes)")
                                .arg(QString::fromStdString(c.designator), QString::fromStdString(d->name))
                                .arg(d->device->memory_bytes(c.parameters)),
                QVariant::fromValue<qulonglong>(c.id));
        }
    }
    const int index = chips_->findData(QVariant::fromValue<qulonglong>(keep));
    chips_->setCurrentIndex(index >= 0 ? index : 0);
}

model::Id MemoryWindow::current() const
{
    return chips_->currentIndex() < 0 ? model::no_id : chips_->currentData().value<qulonglong>();
}

std::size_t MemoryWindow::size_of(model::Id component) const
{
    const auto* c = document_.schematic().component(component);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    return d && d->device ? d->device->memory_bytes(c->parameters) : 0;
}

std::uint8_t MemoryWindow::fill_of(model::Id component) const
{
    const auto* c = document_.schematic().component(component);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    return d && d->device ? d->device->memory_fill : 0xFF;
}

std::vector<std::uint8_t> MemoryWindow::contents(model::Id component, QString& source) const
{
    const std::size_t size = size_of(component);
    const std::uint8_t fill = fill_of(component);
    if (auto bytes = run_.device_memory(component); bytes && !bytes->empty()) {
        source = run_.active() ? tr("Live contents of the running chip.") : tr("Contents at the end of the last run.");
        bytes->resize(size, fill);
        return *bytes;
    }
    const auto* c = document_.schematic().component(component);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    if (c && d && d->device && !d->device->contents_parameter.empty()) {
        const auto it = c->parameters.find(d->device->contents_parameter);
        if (it != c->parameters.end() && !it->second.empty()) {
            const QString name = QString::fromStdString(it->second);
            const QString path = QDir(document_.base_dir()).absoluteFilePath(name);
            std::string error;
            if (auto bytes = io::read_memory_file(path.toStdWString(), size, fill, error)) {
                source = tr("The contents file %1, which the chip starts each run with.").arg(name);
                return *bytes;
            }
            source = tr("The contents file %1 cannot be read: %2").arg(name, QString::fromStdString(error));
            return std::vector<std::uint8_t>(size, fill);
        }
    }
    source = tr("Every byte 0x%1: the chip starts each run like this; Load... chooses a contents file.")
                 .arg(QStringLiteral("%1").arg(fill, 2, 16, QLatin1Char('0')).toUpper());
    return std::vector<std::uint8_t>(size, fill);
}

void MemoryWindow::refresh()
{
    const model::Id id = current();
    if (id == model::no_id) {
        table_->setRowCount(0);
        bytes_.clear();
        status_->setText(tr("The circuit has no memory chip (a 24C02 EEPROM, for example)."));
        return;
    }
    QString source;
    const auto now_bytes = contents(id, source);
    status_->setText(source);
    const QDateTime now = QDateTime::currentDateTime();
    const bool same_chip = id == shown_ && now_bytes.size() == bytes_.size();
    if (!same_chip) {
        shown_ = id;
        bytes_.assign(now_bytes.size(), 0);
        changed_at_.assign(now_bytes.size(), QDateTime());
        const int rows = static_cast<int>((now_bytes.size() + columns - 1) / columns);
        table_->setRowCount(rows);
        for (int r = 0; r < rows; ++r) {
            table_->setVerticalHeaderItem(r, new QTableWidgetItem(QStringLiteral("%1").arg(r * columns, 4, 16, QLatin1Char('0')).toUpper()));
            for (int c = 0; c <= columns; ++c) {
                auto* item = new QTableWidgetItem;
                item->setTextAlignment(c < columns ? Qt::AlignCenter : Qt::AlignLeft | Qt::AlignVCenter);
                table_->setItem(r, c, item);
            }
        }
    }
    const QColor marker(255, 196, 0, 150);
    for (std::size_t i = 0; i < now_bytes.size(); ++i) {
        const bool changed = !same_chip || now_bytes[i] != bytes_[i];
        if (changed && same_chip) {
            changed_at_[i] = now;
        }
        auto* item = table_->item(static_cast<int>(i / columns), static_cast<int>(i % columns));
        if (changed) {
            item->setText(QStringLiteral("%1").arg(now_bytes[i], 2, 16, QLatin1Char('0')).toUpper());
        }
        const bool recent = changed_at_[i].isValid() && changed_at_[i].msecsTo(now) < highlight_ms;
        item->setBackground(recent ? QBrush(marker) : QBrush());
    }
    // The text column: printable characters, others as dots.
    for (int r = 0; r < table_->rowCount(); ++r) {
        QString text;
        for (int c = 0; c < columns; ++c) {
            const std::size_t i = static_cast<std::size_t>(r * columns + c);
            const auto b = i < now_bytes.size() ? now_bytes[i] : 0;
            text += b >= 0x20 && b < 0x7F ? QChar(b) : QChar('.');
        }
        table_->item(r, columns)->setText(text);
    }
    bytes_ = now_bytes;
}

void MemoryWindow::load()
{
    const model::Id id = current();
    if (id == model::no_id) {
        return;
    }
    const QString chosen = QFileDialog::getOpenFileName(this, tr("Load memory contents"), document_.base_dir(),
        tr("Memory files (*.bin *.hex);;All files (*)"));
    if (chosen.isEmpty()) {
        return;
    }
    std::string error;
    const auto bytes = io::read_memory_file(chosen.toStdWString(), size_of(id), fill_of(id), error);
    if (!bytes) {
        status_->setText(tr("%1 cannot be loaded: %2").arg(QDir::toNativeSeparators(chosen), QString::fromStdString(error)));
        return;
    }
    if (run_.active()) {
        run_.load_device_memory(id, *bytes);
        status_->setText(tr("Loaded into the running chip."));
        return;
    }
    // Nothing runs: the file becomes the chip's contents file, relative to the project.
    const auto* c = document_.schematic().component(id);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    if (!d || !d->device || d->device->contents_parameter.empty()) {
        return;
    }
    const QString dir = document_.base_dir();
    const QString stored = dir.isEmpty() ? chosen : QDir(dir).relativeFilePath(chosen);
    document_.apply(editing::set_parameter(document_.schematic(), id, d->device->contents_parameter,
        QDir::fromNativeSeparators(stored).toStdString()));
    refresh();
}

void MemoryWindow::save()
{
    const model::Id id = current();
    if (id == model::no_id) {
        return;
    }
    const QString chosen = QFileDialog::getSaveFileName(this, tr("Save memory contents"), document_.base_dir(),
        tr("Binary (*.bin);;Intel HEX (*.hex)"));
    if (chosen.isEmpty()) {
        return;
    }
    QString source;
    std::string error;
    if (!io::write_memory_file(chosen.toStdWString(), contents(id, source), error)) {
        status_->setText(tr("%1 cannot be written: %2").arg(QDir::toNativeSeparators(chosen), QString::fromStdString(error)));
        return;
    }
    status_->setText(tr("Saved to %1.").arg(QDir::toNativeSeparators(chosen)));
}

} // namespace simnodus::ui
