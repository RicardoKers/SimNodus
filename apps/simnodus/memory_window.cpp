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
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace simnodus::ui {

namespace {

constexpr int columns = MemoryModel::columns;
constexpr qint64 highlight_ms = 1500;
constexpr std::size_t max_changes = 4096; // runs of changed bytes remembered for the highlight

} // namespace

void MemoryModel::set_bytes(std::vector<std::uint8_t> bytes, bool fresh)
{
    if (fresh || bytes.size() != bytes_.size()) {
        beginResetModel();
        bytes_ = std::move(bytes);
        changes_.clear();
        endResetModel();
        return;
    }
    if (bytes == bytes_) {
        expire();
        return;
    }
    // Runs of bytes that differ, each one entry; the rows between the first
    // and the last of them are painted again.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::size_t first = bytes.size();
    std::size_t last = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == bytes_[i]) {
            continue;
        }
        std::size_t end = i;
        while (end + 1 < bytes.size() && bytes[end + 1] != bytes_[end + 1]) {
            ++end;
        }
        if (changes_.size() < max_changes) {
            changes_.push_back({i, end, now});
        }
        first = std::min(first, i);
        last = end;
        i = end;
    }
    bytes_ = std::move(bytes);
    rows_changed(first, last);
    expire();
}

bool MemoryModel::expire()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::size_t first = bytes_.size();
    std::size_t last = 0;
    std::erase_if(changes_, [&](const Change& c) {
        if (now - c.at < highlight_ms) {
            return false;
        }
        first = std::min(first, c.first);
        last = std::max(last, c.last);
        return true;
    });
    if (first <= last && first < bytes_.size()) {
        rows_changed(first, last);
    }
    return !changes_.empty();
}

bool MemoryModel::recent(std::size_t index) const
{
    return std::any_of(changes_.begin(), changes_.end(),
        [index](const Change& c) { return index >= c.first && index <= c.last; });
}

void MemoryModel::rows_changed(std::size_t first, std::size_t last)
{
    emit dataChanged(index(static_cast<int>(first / columns), 0), index(static_cast<int>(last / columns), columns));
}

int MemoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>((bytes_.size() + columns - 1) / columns);
}

int MemoryModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : columns + 1;
}

QVariant MemoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const std::size_t row = static_cast<std::size_t>(index.row()) * columns;
    if (index.column() == columns) { // the text column: printable characters, others as dots
        if (role == Qt::DisplayRole) {
            QString text;
            for (std::size_t i = row; i < row + columns; ++i) {
                const auto b = i < bytes_.size() ? bytes_[i] : 0;
                text += b >= 0x20 && b < 0x7F ? QChar(b) : QChar('.');
            }
            return text;
        }
        return role == Qt::TextAlignmentRole ? QVariant(static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter)) : QVariant();
    }
    const std::size_t i = row + static_cast<std::size_t>(index.column());
    if (i >= bytes_.size()) {
        return {};
    }
    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("%1").arg(bytes_[i], 2, 16, QLatin1Char('0')).toUpper();
    case Qt::TextAlignmentRole:
        return static_cast<int>(Qt::AlignCenter);
    case Qt::BackgroundRole:
        return recent(i) ? QVariant(QBrush(QColor(255, 196, 0, 150))) : QVariant();
    default:
        return {};
    }
}

QVariant MemoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (orientation == Qt::Horizontal) {
        return section < columns ? QStringLiteral("%1").arg(section, 1, 16).toUpper() : tr("Text");
    }
    // The row's address, wide enough for the last one.
    const int digits = bytes_.size() > 0x10000 ? 6 : 4;
    return QStringLiteral("%1").arg(section * columns, digits, 16, QLatin1Char('0')).toUpper();
}

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

    model_ = new MemoryModel(this);
    table_ = new QTableView;
    table_->setObjectName(QStringLiteral("memoryTable"));
    table_->setModel(model_);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Fixed widths: sizing columns to their contents would look at every row.
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    const int byte_width = QFontMetrics(table_->font()).horizontalAdvance(QStringLiteral("00")) + 14;
    for (int c = 0; c < columns; ++c) {
        table_->setColumnWidth(c, byte_width);
    }
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
        if (d && d->mcu && avr(*d)) { // a bare AVR chip's EEPROM (E-10)
            chips_->addItem(QStringLiteral("%1 (%2 EEPROM, %3 bytes)")
                                .arg(QString::fromStdString(c.designator), QString::fromStdString(d->name))
                                .arg(avr_eeprom_bytes(*d)),
                QVariant::fromValue<qulonglong>(c.id));
            continue;
        }
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
    if (d && d->mcu && avr(*d)) {
        return avr_eeprom_bytes(*d);
    }
    return d && d->device ? d->device->memory_bytes(c->parameters) : 0;
}

std::uint8_t MemoryWindow::fill_of(model::Id component) const
{
    const auto* c = document_.schematic().component(component);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    return d && d->device ? d->device->memory_fill : 0xFF;
}

std::string MemoryWindow::contents_parameter(model::Id component) const
{
    const auto* c = document_.schematic().component(component);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    if (d && d->mcu && avr(*d)) {
        return "eeprom";
    }
    return d && d->device ? d->device->contents_parameter : std::string();
}

std::vector<std::uint8_t> MemoryWindow::contents(model::Id component, QString& source) const
{
    const std::size_t size = size_of(component);
    const std::uint8_t fill = fill_of(component);
    const auto* shown = document_.schematic().component(component);
    const auto* kind = shown ? document_.library().find(shown->type) : nullptr;
    const model::Id memory = kind && kind->mcu && avr(*kind) ? sim::SimulationSession::eeprom_memory(component) : component;
    if (auto bytes = run_.device_memory(memory); bytes && !bytes->empty()) {
        source = run_.active() ? tr("Live contents of the running chip.") : tr("Contents at the end of the last run.");
        bytes->resize(size, fill);
        return *bytes;
    }
    const auto* c = document_.schematic().component(component);
    const std::string parameter = contents_parameter(component);
    if (c && !parameter.empty()) {
        const auto it = c->parameters.find(parameter);
        if (it != c->parameters.end() && !it->second.empty()) {
            const QString name = QString::fromStdString(it->second);
            const QString path = QDir(document_.base_dir()).absoluteFilePath(name);
            std::string error;
            const QFileInfo info(path);
            if (file_.path != path || file_.modified != info.lastModified() || file_.size != info.size()
                || file_.bytes != size || file_.fill != fill) {
                file_ = {path, info.lastModified(), info.size(), size, fill, {}, {}};
                if (auto bytes = io::read_memory_file(path.toStdWString(), size, fill, error)) {
                    file_.contents = std::move(*bytes);
                } else {
                    file_.error = QString::fromStdString(error);
                }
            }
            if (file_.contents.size() == size) {
                source = tr("The contents file %1, which the chip starts each run with.").arg(name);
                return file_.contents;
            }
            source = tr("The contents file %1 cannot be read: %2").arg(name, file_.error);
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
        model_->set_bytes({}, true);
        shown_ = model::no_id;
        status_->setText(tr("The circuit has no memory chip (a 24C02 EEPROM, or a microcontroller's EEPROM, for example)."));
        return;
    }
    QString source;
    auto now_bytes = contents(id, source);
    status_->setText(source);
    model_->set_bytes(std::move(now_bytes), id != shown_);
    shown_ = id;
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
    const auto* c = document_.schematic().component(id);
    const auto* d = c ? document_.library().find(c->type) : nullptr;
    const bool chip_eeprom = d && d->mcu && avr(*d);
    if (run_.active()) {
        if (chip_eeprom) {
            status_->setText(tr("Stop the simulation to load the EEPROM: the chip reads it when it starts."));
            return;
        }
        run_.load_device_memory(id, *bytes);
        status_->setText(tr("Loaded into the running chip."));
        return;
    }
    // Nothing runs: the file becomes the chip's contents file, relative to the project.
    const std::string parameter = contents_parameter(id);
    if (parameter.empty()) {
        return;
    }
    if (chip_eeprom && !chosen.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive)) {
        // The chip reads raw bytes: a HEX file (an .eep) is written as its binary image.
        if (document_.base_dir().isEmpty()) {
            status_->setText(tr("Save the project first: the EEPROM is kept as a file beside it."));
            return;
        }
        const QString name = file_name_part(QString::fromStdString(c->designator)) + QStringLiteral("-eeprom.bin");
        if (!io::write_memory_file(QDir(document_.base_dir()).absoluteFilePath(name).toStdWString(), *bytes, error)) {
            status_->setText(tr("%1 cannot be written: %2").arg(name, QString::fromStdString(error)));
            return;
        }
        document_.apply(editing::set_parameter(document_.schematic(), id, parameter, name.toStdString()));
        refresh();
        return;
    }
    const QString dir = document_.base_dir();
    const QString stored = dir.isEmpty() ? chosen : QDir(dir).relativeFilePath(chosen);
    document_.apply(editing::set_parameter(document_.schematic(), id, parameter, QDir::fromNativeSeparators(stored).toStdString()));
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
