// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "register_window.hpp"

#include "document.hpp"
#include "run_controller.hpp"
#include "sim/simulation_session.hpp"

#include <QAbstractItemDelegate>
#include <QBrush>
#include <QComboBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVBoxLayout>

namespace simnodus::ui {

namespace {

constexpr qint64 mark_ms = 1500; // how long a changed value stays marked

QString hex(std::uint32_t value, unsigned bits)
{
    return QStringLiteral("0x%1").arg(value, static_cast<int>((bits + 3) / 4), 16, QLatin1Char('0')).toUpper().replace(
        QStringLiteral("0X"), QStringLiteral("0x"));
}

// The bits in groups of four, highest first.
QString binary(std::uint32_t value, unsigned bits)
{
    QString out;
    for (unsigned i = bits; i-- > 0;) {
        out += (value >> i) & 1u ? QLatin1Char('1') : QLatin1Char('0');
        if (i % 4 == 0 && i != 0) {
            out += QLatin1Char(' ');
        }
    }
    return out;
}

std::uint32_t field_mask(const mcu::RegisterField& f)
{
    return (f.width >= 32 ? 0xFFFFFFFFu : ((1u << f.width) - 1u)) << f.lsb;
}

// The editor of a value gets the value once: a counter that goes on counting
// behind it (every change sets the editor's data again) would type over the user.
class ValueDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (!editor->property("simnodusFilled").toBool()) {
            QStyledItemDelegate::setEditorData(editor, index);
            editor->setProperty("simnodusFilled", true);
        }
    }
};

} // namespace

RegisterModel::RegisterModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

quintptr RegisterModel::pack(const Place& place)
{
    // Ten bits a level, each one more than the row (0: none): fits 32 bits.
    return static_cast<quintptr>(place.peripheral + 1) | static_cast<quintptr>(place.reg + 1) << 10
        | static_cast<quintptr>(place.field + 1) << 20;
}

RegisterModel::Place RegisterModel::unpack(quintptr id)
{
    return {static_cast<int>(id & 0x3FF) - 1, static_cast<int>((id >> 10) & 0x3FF) - 1,
        static_cast<int>((id >> 20) & 0x3FF) - 1};
}

void RegisterModel::set_map(const mcu::RegisterMap* map)
{
    if (map == map_) {
        return;
    }
    beginResetModel();
    map_ = map;
    states_.clear();
    if (map_) {
        for (const auto& p : map_->peripherals) {
            states_.emplace_back(p.registers.size());
        }
    }
    endResetModel();
}

void RegisterModel::set_values(const mcu::RegisterBytes& bytes)
{
    if (!map_) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (std::size_t p = 0; p < map_->peripherals.size(); ++p) {
        const auto& peripheral = map_->peripherals[p];
        for (std::size_t r = 0; r < peripheral.registers.size(); ++r) {
            const auto& reg = peripheral.registers[r];
            State& state = states_[p][r];
            std::uint32_t value = 0;
            const bool known = bytes.value(peripheral.base + reg.offset, reg.size, value);
            if (known == state.known && (!known || value == state.value)) {
                continue;
            }
            if (known && state.known) { // a first value is not a change
                state.changed = value ^ state.value;
                state.changed_at = now;
            }
            state.known = known;
            state.value = value;
            const QModelIndex row = index(static_cast<int>(r), 0, index(static_cast<int>(p), 0));
            emit dataChanged(row, row.siblingAtColumn(ColumnCount - 1));
            if (!reg.fields.empty()) {
                emit dataChanged(index(0, 0, row), index(static_cast<int>(reg.fields.size()) - 1, ColumnCount - 1, row));
            }
        }
    }
}

void RegisterModel::clear_values()
{
    set_values({});
}

bool RegisterModel::expire()
{
    if (!map_) {
        return false;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool any = false;
    for (std::size_t p = 0; p < states_.size(); ++p) {
        for (std::size_t r = 0; r < states_[p].size(); ++r) {
            State& state = states_[p][r];
            if (state.changed_at != 0 && now - state.changed_at > mark_ms) {
                state.changed_at = 0;
                state.changed = 0;
                any = true;
                const QModelIndex row = index(static_cast<int>(r), 0, index(static_cast<int>(p), 0));
                emit dataChanged(row, row.siblingAtColumn(ColumnCount - 1), {Qt::BackgroundRole});
                if (const auto fields = map_->peripherals[p].registers[r].fields.size(); fields > 0) {
                    emit dataChanged(index(0, 0, row), index(static_cast<int>(fields) - 1, ColumnCount - 1, row),
                        {Qt::BackgroundRole});
                }
            }
        }
    }
    return any;
}

bool RegisterModel::recent(const State& state, std::uint32_t mask) const
{
    return state.changed_at != 0 && (state.changed & mask) != 0;
}

bool RegisterModel::value(const QString& peripheral, const QString& reg, std::uint32_t& out) const
{
    const QModelIndex at = index_of(peripheral, reg);
    if (!at.isValid()) {
        return false;
    }
    const Place where = place(at);
    const State& state = states_[static_cast<std::size_t>(where.peripheral)][static_cast<std::size_t>(where.reg)];
    out = state.value;
    return state.known;
}

QModelIndex RegisterModel::index_of(const QString& peripheral, const QString& reg, const QString& field) const
{
    if (!map_) {
        return {};
    }
    for (std::size_t p = 0; p < map_->peripherals.size(); ++p) {
        if (QString::fromStdString(map_->peripherals[p].name) != peripheral) {
            continue;
        }
        const QModelIndex top = index(static_cast<int>(p), 0);
        if (reg.isEmpty()) {
            return top;
        }
        const auto& registers = map_->peripherals[p].registers;
        for (std::size_t r = 0; r < registers.size(); ++r) {
            if (QString::fromStdString(registers[r].name) != reg) {
                continue;
            }
            const QModelIndex row = index(static_cast<int>(r), 0, top);
            if (field.isEmpty()) {
                return row;
            }
            for (std::size_t f = 0; f < registers[r].fields.size(); ++f) {
                if (QString::fromStdString(registers[r].fields[f].name) == field) {
                    return index(static_cast<int>(f), 0, row);
                }
            }
        }
    }
    return {};
}

void RegisterModel::set_editable(bool editable)
{
    editable_ = editable;
}

bool RegisterModel::parse_value(const QString& text, std::uint32_t& out)
{
    QString t = text.trimmed();
    t.remove(QLatin1Char(' '));
    t.remove(QLatin1Char('_'));
    int base = 10;
    if (t.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        base = 16;
        t = t.mid(2);
    } else if (t.startsWith(QStringLiteral("0b"), Qt::CaseInsensitive)) {
        base = 2;
        t = t.mid(2);
    }
    bool ok = false;
    const qulonglong value = t.toULongLong(&ok, base);
    if (!ok || value > 0xFFFFFFFFull) {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

Qt::ItemFlags RegisterModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    const Place where = place(index);
    if (editable_ && map_ && index.column() == Value && where.reg >= 0
        && states_[static_cast<std::size_t>(where.peripheral)][static_cast<std::size_t>(where.reg)].known) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool RegisterModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable)) {
        return false;
    }
    const Place where = place(index);
    const auto& peripheral = map_->peripherals[static_cast<std::size_t>(where.peripheral)];
    const auto& reg = peripheral.registers[static_cast<std::size_t>(where.reg)];
    const State& state = states_[static_cast<std::size_t>(where.peripheral)][static_cast<std::size_t>(where.reg)];
    std::uint32_t typed = 0;
    if (!parse_value(value.toString(), typed)) {
        refusal_ = tr("A value is a number: 26, 0x1A or 0b11010.");
        return false;
    }
    const unsigned bits = where.field < 0 ? reg.size * 8 : reg.fields[static_cast<std::size_t>(where.field)].width;
    if (bits < 32 && typed >= (1u << bits)) {
        refusal_ = tr("%1 does not fit in %n bit(s).", nullptr, static_cast<int>(bits)).arg(value.toString().trimmed());
        return false;
    }
    std::uint32_t whole = typed;
    if (where.field >= 0) {
        const auto& field = reg.fields[static_cast<std::size_t>(where.field)];
        whole = (state.value & ~field_mask(field)) | (typed << field.lsb);
    }
    refusal_.clear();
    emit register_edited(peripheral.base + reg.offset, static_cast<int>(reg.size), whole);
    return true;
}

QModelIndex RegisterModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!map_ || row < 0 || column < 0 || column >= ColumnCount || row >= rowCount(parent)) {
        return {};
    }
    Place where = place(parent);
    if (where.peripheral < 0) {
        where.peripheral = row;
    } else if (where.reg < 0) {
        where.reg = row;
    } else {
        where.field = row;
    }
    return createIndex(row, column, pack(where));
}

QModelIndex RegisterModel::parent(const QModelIndex& child) const
{
    Place where = place(child);
    if (where.field >= 0) {
        where.field = -1;
        return createIndex(where.reg, 0, pack(where));
    }
    if (where.reg >= 0) {
        where.reg = -1;
        return createIndex(where.peripheral, 0, pack(where));
    }
    return {};
}

int RegisterModel::rowCount(const QModelIndex& parent) const
{
    if (!map_ || parent.column() > 0) {
        return 0;
    }
    const Place where = place(parent);
    if (where.peripheral < 0) {
        return static_cast<int>(map_->peripherals.size());
    }
    const auto& peripheral = map_->peripherals[static_cast<std::size_t>(where.peripheral)];
    if (where.reg < 0) {
        return static_cast<int>(peripheral.registers.size());
    }
    return where.field < 0 ? static_cast<int>(peripheral.registers[static_cast<std::size_t>(where.reg)].fields.size()) : 0;
}

int RegisterModel::columnCount(const QModelIndex&) const
{
    return ColumnCount;
}

QVariant RegisterModel::data(const QModelIndex& index, int role) const
{
    if (!map_ || !index.isValid()) {
        return {};
    }
    const Place where = place(index);
    const auto& peripheral = map_->peripherals[static_cast<std::size_t>(where.peripheral)];
    if (where.reg < 0) { // a peripheral
        if (role == Qt::DisplayRole) {
            return index.column() == Name          ? QString::fromStdString(peripheral.name)
                : index.column() == Description ? QString::fromStdString(peripheral.description)
                                                : QVariant();
        }
        return role == Qt::ToolTipRole ? QVariant(QString::fromStdString(peripheral.description)) : QVariant();
    }
    const auto& reg = peripheral.registers[static_cast<std::size_t>(where.reg)];
    const State& state = states_[static_cast<std::size_t>(where.peripheral)][static_cast<std::size_t>(where.reg)];
    if (where.field < 0) { // a register
        switch (role) {
        case Qt::EditRole: // the editor opens with the value
            return index.column() == Value && state.known ? QVariant(hex(state.value, reg.size * 8)) : QVariant();
        case Qt::DisplayRole:
            switch (index.column()) {
            case Name: return QString::fromStdString(reg.name);
            case Value: return state.known ? hex(state.value, reg.size * 8) : QStringLiteral("-");
            case Bits: return state.known ? binary(state.value, reg.size * 8) : QString();
            default: return QString::fromStdString(reg.description);
            }
        case Qt::ToolTipRole:
            return tr("%1 at %2: %3")
                .arg(QString::fromStdString(peripheral.name + "." + reg.name), hex(peripheral.base + reg.offset, 32),
                    QString::fromStdString(reg.description));
        case Qt::BackgroundRole:
            return recent(state, 0xFFFFFFFFu) && index.column() != Description ? QVariant(QBrush(QColor(255, 196, 0, 150)))
                                                                               : QVariant();
        case Qt::FontRole:
            return index.column() == Value || index.column() == Bits
                ? QVariant(QFontDatabase::systemFont(QFontDatabase::FixedFont))
                : QVariant();
        default:
            return {};
        }
    }
    const auto& field = reg.fields[static_cast<std::size_t>(where.field)];
    switch (role) {
    case Qt::EditRole:
        return index.column() == Value && state.known ? QVariant(QString::number(mcu::field_value(state.value, field)))
                                                      : QVariant();
    case Qt::DisplayRole:
        switch (index.column()) {
        case Name: return QString::fromStdString(field.name);
        case Value: {
            if (!state.known) {
                return QStringLiteral("-");
            }
            const std::uint32_t value = mcu::field_value(state.value, field);
            // One bit as it is; more as a number with its hexadecimal form when that says more.
            return field.width == 1 || value < 10 ? QString::number(value)
                                                  : QStringLiteral("%1 (%2)").arg(value).arg(hex(value, field.width));
        }
        case Bits:
            return field.width == 1 ? QStringLiteral("[%1]").arg(field.lsb)
                                    : QStringLiteral("[%1:%2]").arg(field.lsb + field.width - 1).arg(field.lsb);
        default: return QString::fromStdString(field.description);
        }
    case Qt::ToolTipRole:
        return field.description.empty() ? QVariant() : QVariant(QString::fromStdString(field.description));
    case Qt::BackgroundRole:
        return recent(state, field_mask(field)) && index.column() != Description
            ? QVariant(QBrush(QColor(255, 196, 0, 150)))
            : QVariant();
    case Qt::FontRole:
        return index.column() == Value || index.column() == Bits
            ? QVariant(QFontDatabase::systemFont(QFontDatabase::FixedFont))
            : QVariant();
    default:
        return {};
    }
}

QVariant RegisterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case Name: return tr("Register");
    case Value: return tr("Value");
    case Bits: return tr("Bits");
    default: return tr("Description");
    }
}

RegisterWindow::RegisterWindow(Document& document, RunController& run, QWidget* parent)
    : QWidget(parent, Qt::Window), document_(document), run_(run)
{
    setObjectName(QStringLiteral("registerWindow"));
    setWindowTitle(tr("Register Viewer - SimNodus"));
    setAttribute(Qt::WA_QuitOnClose, false);
    resize(860, 600);

    boards_ = new QComboBox;
    boards_->setObjectName(QStringLiteral("registerBoards"));
    filter_ = new QLineEdit;
    filter_->setObjectName(QStringLiteral("registerFilter"));
    filter_->setPlaceholderText(tr("Filter (GPIOC, CR1, RXNE)"));
    filter_->setToolTip(tr("Shows only the peripherals, registers and fields whose name has this text"));
    filter_->setClearButtonEnabled(true);
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Microcontroller:")));
    top->addWidget(boards_, 1);
    top->addWidget(new QLabel(tr("Filter:")));
    top->addWidget(filter_, 1);

    model_ = new RegisterModel(this);
    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setFilterKeyColumn(RegisterModel::Name);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setRecursiveFilteringEnabled(true); // a peripheral stays for its register that matches
    proxy_->setAutoAcceptChildRows(true);       // and a match shows all that is below it
    tree_ = new QTreeView;
    tree_->setObjectName(QStringLiteral("registerTree"));
    tree_->setModel(proxy_);
    tree_->setUniformRowHeights(true);
    tree_->setAlternatingRowColors(true);
    // A double-click (or F2) on a value edits it while a run is on.
    tree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    tree_->setItemDelegate(new ValueDelegate(tree_));
    tree_->header()->setStretchLastSection(true);
    const QFontMetrics metrics(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    tree_->setColumnWidth(RegisterModel::Name, 190);
    tree_->setColumnWidth(RegisterModel::Value, metrics.horizontalAdvance(QStringLiteral("0x00000000")) + 30);
    tree_->setColumnWidth(RegisterModel::Bits,
        metrics.horizontalAdvance(QStringLiteral("0000 0000 0000 0000 0000 0000 0000 0000")) + 20);

    status_ = new QLabel;
    status_->setObjectName(QStringLiteral("registerStatus"));
    status_->setWordWrap(true);
    note_ = new QLabel;
    note_->setObjectName(QStringLiteral("registerNote"));
    note_->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(tree_, 1);
    layout->addWidget(note_);
    layout->addWidget(status_);

    connect(model_, &RegisterModel::register_edited, this, &RegisterWindow::edit);
    // A value that was refused says why (the delegate itself only closes the editor).
    connect(tree_->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this] {
        if (!model_->refusal().isEmpty()) {
            note_->setText(model_->refusal());
        }
    });

    connect(boards_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(filter_, &QLineEdit::textChanged, this, [this](const QString& text) {
        proxy_->setFilterFixedString(text.trimmed());
        if (!text.trimmed().isEmpty()) {
            tree_->expandToDepth(0); // the registers that match, without their fields
        }
    });
    connect(&document_, &Document::changed, this, [this] {
        if (isVisible()) {
            rebuild_list();
        }
    });
    timer_.setInterval(200);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (isVisible()) {
            refresh();
        }
    });
    timer_.start();
}

void RegisterWindow::show_board(model::Id component)
{
    rebuild_list();
    if (component != model::no_id) {
        const int index = boards_->findData(QVariant::fromValue<qulonglong>(component));
        if (index >= 0) {
            boards_->setCurrentIndex(index);
        }
    }
    show();
    raise();
    activateWindow();
    refresh();
}

void RegisterWindow::hideEvent(QHideEvent* event)
{
    watch(false); // nobody looks: the engine stops copying them
    QWidget::hideEvent(event);
}

void RegisterWindow::rebuild_list()
{
    const model::Id keep = current();
    QSignalBlocker block(boards_);
    boards_->clear();
    const auto& library = document_.library();
    for (const auto& c : document_.schematic().components()) {
        const auto* d = library.find(c.type);
        if (d && d->mcu) {
            boards_->addItem(QStringLiteral("%1 (%2)").arg(QString::fromStdString(c.designator), QString::fromStdString(d->name)),
                QVariant::fromValue<qulonglong>(c.id));
        }
    }
    const int index = boards_->findData(QVariant::fromValue<qulonglong>(keep));
    boards_->setCurrentIndex(index >= 0 ? index : 0);
}

model::Id RegisterWindow::current() const
{
    return boards_->currentIndex() < 0 ? model::no_id : boards_->currentData().value<qulonglong>();
}

void RegisterWindow::watch(bool on)
{
    const model::Id want = on ? shown_ : model::no_id;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!run_.active()) {
        watched_ = model::no_id; // the next run is asked again
        return;
    }
    if (watched_ != model::no_id && watched_ != want) {
        run_.watch_registers(watched_, {});
        watched_ = model::no_id;
    }
    if (want == model::no_id || !model_->map()) {
        return;
    }
    // Asked once; again every half second while the answer does not come (a
    // run that started after the window was opened, an engine that restarted).
    const bool answered = run_.device_memory(sim::Session::registers_memory(want)).has_value();
    if (watched_ == want && (answered || now - watch_sent_ < 500)) {
        return;
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    for (const auto& p : model_->map()->peripherals) {
        const auto more = mcu::register_ranges(p);
        ranges.insert(ranges.end(), more.begin(), more.end());
    }
    run_.watch_registers(want, ranges);
    watched_ = want;
    watch_sent_ = now;
}

void RegisterWindow::edit(quint32 address, int size, quint32 value)
{
    if (shown_ == model::no_id || !run_.active()) {
        note_->setText(tr("Registers are written in the chip of a running simulation (paused or not)."));
        return;
    }
    run_.write_register(shown_, address, static_cast<unsigned>(size), value);
    note_->setText(tr("%1 written at %2. What the chip made of it shows above.")
            .arg(hex(value, static_cast<unsigned>(size) * 8), hex(address, 32)));
}

void RegisterWindow::refresh()
{
    const model::Id id = current();
    const auto* component = id == model::no_id ? nullptr : document_.schematic().component(id);
    const auto* kind = component ? document_.library().find(component->type) : nullptr;
    const mcu::RegisterMap* map = kind && kind->mcu ? mcu::register_map(kind->mcu->chip) : nullptr;
    if (id != shown_ || map != model_->map()) {
        shown_ = id;
        model_->set_map(map);
    }
    if (id == model::no_id) {
        status_->setText(tr("The circuit has no microcontroller."));
        return;
    }
    if (!map) {
        status_->setText(tr("%1 has no register view yet.").arg(QString::fromStdString(kind ? kind->name : std::string())));
        return;
    }
    watch(true);
    model_->set_editable(run_.active());
    const auto blob = run_.device_memory(sim::Session::registers_memory(id));
    if (blob) {
        model_->set_values(mcu::RegisterBytes::parse(*blob));
    } else {
        model_->clear_values();
    }
    model_->expire();
    status_->setText(!blob ? tr("The registers show their values while a simulation runs.")
            : run_.active() ? tr("Values of the running simulation; a value that changed is marked for a moment. "
                                 "They are looked at without the effects of a read by the firmware. "
                                 "Double-click a value to change it.")
                            : tr("Values as the last simulation left them."));
}

} // namespace simnodus::ui
