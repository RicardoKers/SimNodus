// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "component_panel.hpp"

#include "symbol_painter.hpp"
#include "theme.hpp"

#include "library/component_library.hpp"

#include <QCoreApplication>
#include <QDrag>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QMimeData>
#include <QPainter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace simnodus::ui {
namespace {

enum Role { TypeRole = Qt::UserRole };

QString translated(const std::string& text)
{
    // Library texts are data; a translation file can provide them later (UI-16).
    return QCoreApplication::translate("ComponentLibrary", text.c_str());
}

class ComponentTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    void startDrag(Qt::DropActions) override
    {
        const auto* item = currentItem();
        if (!item || item->data(0, TypeRole).toString().isEmpty()) {
            return;
        }
        auto* mime = new QMimeData;
        mime->setData(QStringLiteral("application/x-simnodus-component"), item->data(0, TypeRole).toString().toUtf8());
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
    }
};

} // namespace

class SymbolPreview : public QWidget {
public:
    explicit SymbolPreview(QWidget* parent = nullptr) : QWidget(parent) { setMinimumSize(90, 80); }

    void set_definition(const library::ComponentDefinition* definition)
    {
        definition_ = definition;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), theme::canvas_background);
        if (!definition_) {
            return;
        }
        const QRectF bounds = symbol_bounds(*definition_);
        const double scale = std::min((width() - 12) / bounds.width(), (height() - 12) / bounds.height());
        painter.translate(width() / 2.0, height() / 2.0);
        painter.scale(std::min(scale, 14.0), std::min(scale, 14.0));
        painter.translate(-bounds.center());
        SymbolStyle style;
        style.ink = theme::ink;
        style.line_width = 0.14;
        style.state = definition_->state_graphics.empty() ? QString() : QStringLiteral("0");
        style.draw_pins = true;
        paint_symbol(painter, *definition_, style);
    }

private:
    const library::ComponentDefinition* definition_ = nullptr;
};

ComponentPanel::ComponentPanel(const library::ComponentLibrary& library, QWidget* parent)
    : QWidget(parent), library_(&library)
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

    tree_ = new ComponentTree;
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(true);
    tree_->setUniformRowHeights(true);
    tree_->setFrameShape(QFrame::NoFrame);
    tree_->setDragEnabled(true);
    tree_->setDragDropMode(QAbstractItemView::DragOnly);
    tree_->setObjectName(QStringLiteral("componentTree"));
    layout->addWidget(tree_, 1);

    layout->addWidget(theme::make_caption(tr("Preview")));
    auto* preview = new QFrame;
    preview->setObjectName(QStringLiteral("componentPreview"));
    preview->setFrameShape(QFrame::StyledPanel);
    auto* preview_layout = new QHBoxLayout(preview);
    preview_symbol_ = new SymbolPreview;
    preview_layout->addWidget(preview_symbol_);
    auto* text = new QVBoxLayout;
    preview_name_ = new QLabel;
    QFont bold = preview_name_->font();
    bold.setBold(true);
    preview_name_->setFont(bold);
    preview_name_->setWordWrap(true);
    preview_details_ = new QLabel;
    preview_details_->setWordWrap(true);
    preview_details_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    text->addWidget(preview_name_);
    text->addWidget(preview_details_, 1);
    preview_layout->addLayout(text, 1);
    layout->addWidget(preview);

    populate();
    connect(search_, &QLineEdit::textChanged, this, &ComponentPanel::apply_filter);
    connect(tree_, &QTreeWidget::currentItemChanged, this, &ComponentPanel::update_preview);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        const QString type = item->data(0, TypeRole).toString();
        if (!type.isEmpty()) {
            emit placement_requested(type);
        }
    });
    // Down arrow or Enter in the search field moves into the tree (keyboard navigation).
    search_->installEventFilter(this);
    update_preview();
}

void ComponentPanel::populate()
{
    for (const auto& category : library_->categories()) {
        auto* parent = new QTreeWidgetItem(tree_, {translated(category)});
        parent->setFlags(Qt::ItemIsEnabled);
        for (const auto& d : library_->definitions()) {
            if (d.category != category) {
                continue;
            }
            auto* item = new QTreeWidgetItem(parent, {translated(d.name)});
            item->setData(0, TypeRole, QString::fromStdString(d.type));
            item->setToolTip(0, translated(d.description));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        }
        parent->setExpanded(true);
    }
}

void ComponentPanel::set_library(const library::ComponentLibrary& library)
{
    const QString current = selected_type();
    library_ = &library;
    tree_->clear();
    populate();
    apply_filter(search_->text());
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if (!current.isEmpty() && (*it)->data(0, TypeRole).toString() == current) {
            tree_->setCurrentItem(*it);
            break;
        }
    }
    update_preview();
}

void ComponentPanel::apply_filter(const QString& text)
{
    const QString needle = text.trimmed();
    // The part chosen for Enter: an exact name or type first, then a name with a
    // word starting with the text ("LED" picks the LED, not "Current-controlled").
    QTreeWidgetItem* first_match = nullptr;
    int best_rank = 3;
    const QRegularExpression word_start(QStringLiteral("\\b") +QRegularExpression::escape(needle),
        QRegularExpression::CaseInsensitiveOption);
    for (int c = 0; c < tree_->topLevelItemCount(); ++c) {
        auto* category = tree_->topLevelItem(c);
        int visible = 0;
        for (int i = 0; i < category->childCount(); ++i) {
            auto* item = category->child(i);
            const auto* d = library_->find(item->data(0, TypeRole).toString().toStdString());
            const bool match = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive)
                || (d && QString::fromStdString(d->description).contains(needle, Qt::CaseInsensitive))
                || (d && QString::fromStdString(d->type).contains(needle, Qt::CaseInsensitive))
                || category->text(0).contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
            if (match) {
                ++visible;
                const bool exact = item->text(0).compare(needle, Qt::CaseInsensitive) == 0
                    || (d && QString::fromStdString(d->type).compare(needle, Qt::CaseInsensitive) == 0);
                const int rank = exact ? 0 : word_start.match(item->text(0)).hasMatch() ? 1 : 2;
                if (rank < best_rank) {
                    best_rank = rank;
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
    const auto* d = item ? library_->find(item->data(0, TypeRole).toString().toStdString()) : nullptr;
    preview_symbol_->set_definition(d);
    if (!d) {
        preview_name_->setText(tr("No component selected"));
        preview_details_->setText(tr("Select a component to see it. Press Enter, double-click or drag it onto the "
                                     "schematic to place it."));
        return;
    }
    preview_name_->setText(translated(d->name));
    preview_details_->setText(
        tr("%1\nPins: %2").arg(translated(d->description)).arg(static_cast<int>(d->pins.size())));
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
