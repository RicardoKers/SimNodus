// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "component_panel.hpp"

#include "theme.hpp"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <vector>

namespace simnodus::ui {
namespace {

enum Role { TypeRole = Qt::UserRole, DescriptionRole, PinsRole };

struct Entry {
    const char* type;
    const char* name;
    const char* description;
    int pins;
};

struct Category {
    const char* name;
    std::vector<Entry> entries;
};

QString translated(const char* text)
{
    return QCoreApplication::translate("ComponentPanel", text);
}

// Component set A (REQUIREMENTS SIM-06). Strings are marked for translation.
std::vector<Category> catalogue()
{
    return {
        {QT_TRANSLATE_NOOP("ComponentPanel", "Passive"),
            {{"resistor", QT_TRANSLATE_NOOP("ComponentPanel", "Resistor"),
                 QT_TRANSLATE_NOOP("ComponentPanel", "Linear resistor"), 2},
                {"capacitor", QT_TRANSLATE_NOOP("ComponentPanel", "Capacitor"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Ideal capacitor"), 2},
                {"inductor", QT_TRANSLATE_NOOP("ComponentPanel", "Inductor"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Ideal inductor"), 2},
                {"potentiometer", QT_TRANSLATE_NOOP("ComponentPanel", "Potentiometer"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Adjustable voltage divider; drag the wiper while running"), 3}}},
        {QT_TRANSLATE_NOOP("ComponentPanel", "Sources"),
            {{"vdc", QT_TRANSLATE_NOOP("ComponentPanel", "DC voltage source"),
                 QT_TRANSLATE_NOOP("ComponentPanel", "Constant voltage"), 2},
                {"idc", QT_TRANSLATE_NOOP("ComponentPanel", "DC current source"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Constant current"), 2},
                {"battery", QT_TRANSLATE_NOOP("ComponentPanel", "Battery"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Voltage source with internal resistance"), 2},
                {"vpulse", QT_TRANSLATE_NOOP("ComponentPanel", "Pulse generator"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Square or pulse waveform"), 2},
                {"vsine", QT_TRANSLATE_NOOP("ComponentPanel", "Sine generator"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Sinusoidal voltage"), 2},
                {"ground", QT_TRANSLATE_NOOP("ComponentPanel", "Ground"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Reference node 0"), 1}}},
        {QT_TRANSLATE_NOOP("ComponentPanel", "Switches & Controls"),
            {{"spst", QT_TRANSLATE_NOOP("ComponentPanel", "Switch SPST"),
                 QT_TRANSLATE_NOOP("ComponentPanel", "Toggle switch; click to operate while running"), 2},
                {"spdt", QT_TRANSLATE_NOOP("ComponentPanel", "Switch SPDT"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Changeover switch"), 3},
                {"button_no", QT_TRANSLATE_NOOP("ComponentPanel", "Push button (NO)"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Closed while pressed"), 2},
                {"button_nc", QT_TRANSLATE_NOOP("ComponentPanel", "Push button (NC)"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Open while pressed"), 2}}},
        {QT_TRANSLATE_NOOP("ComponentPanel", "Semiconductors"),
            {{"diode", QT_TRANSLATE_NOOP("ComponentPanel", "Diode"),
                 QT_TRANSLATE_NOOP("ComponentPanel", "Silicon diode"), 2},
                {"led", QT_TRANSLATE_NOOP("ComponentPanel", "LED"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Light-emitting diode; glows with its current"), 2},
                {"npn", QT_TRANSLATE_NOOP("ComponentPanel", "NPN transistor"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Bipolar junction transistor"), 3},
                {"nmos", QT_TRANSLATE_NOOP("ComponentPanel", "N-channel MOSFET"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Enhancement-mode MOSFET"), 3}}},
        {QT_TRANSLATE_NOOP("ComponentPanel", "Instruments"),
            {{"voltmeter", QT_TRANSLATE_NOOP("ComponentPanel", "Voltmeter"),
                 QT_TRANSLATE_NOOP("ComponentPanel", "Shows the voltage between its terminals"), 2},
                {"ammeter", QT_TRANSLATE_NOOP("ComponentPanel", "Ammeter"),
                    QT_TRANSLATE_NOOP("ComponentPanel", "Shows the current through it"), 2}}},
        {QT_TRANSLATE_NOOP("ComponentPanel", "Microcontrollers"),
            {{"bluepill", QT_TRANSLATE_NOOP("ComponentPanel", "STM32 Blue Pill"),
                 QT_TRANSLATE_NOOP("ComponentPanel", "STM32F103C8 board; firmware runs in Renode (release 0.2)"), 40}}},
    };
}

} // namespace

ComponentPanel::ComponentPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("componentPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(theme::make_caption(tr("Components")));

    search_ = new QLineEdit;
    search_->setPlaceholderText(tr("Search components"));
    search_->setClearButtonEnabled(true);
    search_->setObjectName(QStringLiteral("componentSearch"));
    layout->addWidget(search_);

    tree_ = new QTreeWidget;
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(true);
    tree_->setUniformRowHeights(true);
    tree_->setFrameShape(QFrame::NoFrame);
    tree_->setObjectName(QStringLiteral("componentTree"));
    layout->addWidget(tree_, 1);

    layout->addWidget(theme::make_caption(tr("Preview")));
    auto* preview = new QFrame;
    preview->setObjectName(QStringLiteral("componentPreview"));
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setMinimumHeight(110);
    auto* preview_layout = new QVBoxLayout(preview);
    preview_name_ = new QLabel;
    QFont bold = preview_name_->font();
    bold.setBold(true);
    preview_name_->setFont(bold);
    preview_details_ = new QLabel;
    preview_details_->setWordWrap(true);
    preview_details_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    preview_layout->addWidget(preview_name_);
    preview_layout->addWidget(preview_details_, 1);
    layout->addWidget(preview);

    populate();
    connect(search_, &QLineEdit::textChanged, this, &ComponentPanel::apply_filter);
    connect(tree_, &QTreeWidget::currentItemChanged, this, &ComponentPanel::update_preview);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        const QString type = item->data(0, TypeRole).toString();
        if (!type.isEmpty()) {
            emit placement_requested(type, item->text(0));
        }
    });
    // Down arrow in the search field moves into the tree (keyboard navigation).
    search_->installEventFilter(this);
    update_preview();
}

void ComponentPanel::populate()
{
    for (const auto& category : catalogue()) {
        auto* parent = new QTreeWidgetItem(tree_, {translated(category.name)});
        parent->setFlags(Qt::ItemIsEnabled);
        for (const auto& entry : category.entries) {
            auto* item = new QTreeWidgetItem(parent, {translated(entry.name)});
            item->setData(0, TypeRole, QString::fromLatin1(entry.type));
            item->setData(0, DescriptionRole, translated(entry.description));
            item->setData(0, PinsRole, entry.pins);
            item->setToolTip(0, translated(entry.description));
        }
        parent->setExpanded(true);
    }
}

void ComponentPanel::apply_filter(const QString& text)
{
    const QString needle = text.trimmed();
    QTreeWidgetItem* first_match = nullptr;
    for (int c = 0; c < tree_->topLevelItemCount(); ++c) {
        auto* category = tree_->topLevelItem(c);
        int visible = 0;
        for (int i = 0; i < category->childCount(); ++i) {
            auto* item = category->child(i);
            const bool match = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive)
                || item->data(0, DescriptionRole).toString().contains(needle, Qt::CaseInsensitive)
                || category->text(0).contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
            if (match) {
                ++visible;
                if (!first_match) {
                    first_match = item;
                }
            }
        }
        category->setHidden(visible == 0);
        if (visible > 0 && !needle.isEmpty()) {
            category->setExpanded(true);
        }
    }
    if (first_match && !needle.isEmpty()) {
        tree_->setCurrentItem(first_match);
    }
}

int ComponentPanel::visible_component_count() const
{
    int count = 0;
    for (int c = 0; c < tree_->topLevelItemCount(); ++c) {
        const auto* category = tree_->topLevelItem(c);
        for (int i = 0; i < category->childCount(); ++i) {
            count += category->child(i)->isHidden() || category->isHidden() ? 0 : 1;
        }
    }
    return count;
}

QString ComponentPanel::selected_type() const
{
    const auto* item = tree_->currentItem();
    return item ? item->data(0, TypeRole).toString() : QString();
}

void ComponentPanel::update_preview()
{
    const auto* item = tree_->currentItem();
    if (!item || item->data(0, TypeRole).toString().isEmpty()) {
        preview_name_->setText(tr("No component selected"));
        preview_details_->setText(tr("Select a component to see its description. Press Enter or double-click "
                                     "to place it."));
        return;
    }
    preview_name_->setText(item->text(0));
    preview_details_->setText(tr("%1\nPins: %2\nSymbol preview arrives with the component library.")
                                  .arg(item->data(0, DescriptionRole).toString())
                                  .arg(item->data(0, PinsRole).toInt()));
}

bool ComponentPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == search_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            tree_->setFocus();
            if (!tree_->currentItem() || tree_->currentItem()->isHidden()) {
                apply_filter(search_->text());
            }
            if (key->key() != Qt::Key_Down && tree_->currentItem()) {
                emit tree_->itemActivated(tree_->currentItem(), 0);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace simnodus::ui
