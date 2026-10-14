// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "messages_panel.hpp"

#include "theme.hpp"

#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace simnodus::ui {

MessagesPanel::MessagesPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("messagesPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* header = new QHBoxLayout;
    header->addWidget(theme::make_caption(tr("Messages")));
    header->addStretch(1);
    auto* clear_button = new QToolButton;
    clear_button->setText(tr("Clear"));
    clear_button->setAutoRaise(true);
    header->addWidget(clear_button);
    layout->addLayout(header);

    list_ = new QTreeWidget;
    list_->setColumnCount(4);
    list_->setHeaderLabels({tr("Time"), tr("Severity"), tr("Message"), tr("Source")});
    list_->setRootIsDecorated(false);
    list_->setUniformRowHeights(true);
    list_->setFrameShape(QFrame::NoFrame);
    list_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    list_->header()->setStretchLastSection(false);
    list_->setObjectName(QStringLiteral("messageList"));
    layout->addWidget(list_);
    connect(clear_button, &QToolButton::clicked, this, &MessagesPanel::clear);
    connect(list_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        std::vector<model::Id> ids;
        for (const auto& v : item->data(0, Qt::UserRole).toList()) {
            ids.push_back(v.value<qulonglong>());
        }
        if (!ids.empty()) {
            emit items_activated(ids);
        }
    });
}

void MessagesPanel::add(Severity severity, const QString& text, const QString& source, std::vector<model::Id> items)
{
    QString label;
    QColor color = theme::ink;
    switch (severity) {
    case Severity::Info:
        label = tr("Info");
        break;
    case Severity::Warning:
        label = tr("Warning");
        color = QColor(0x9a, 0x60, 0x00);
        ++warnings_;
        break;
    case Severity::Error:
        label = tr("Error");
        color = QColor(0xb4, 0x23, 0x18);
        ++errors_;
        break;
    }
    auto* item = new QTreeWidgetItem(list_,
        {QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), label, text, source});
    item->setForeground(1, color);
    item->setToolTip(2, text);
    QVariantList ids;
    for (model::Id id : items) {
        ids << QVariant::fromValue<qulonglong>(id);
    }
    item->setData(0, Qt::UserRole, ids);
    item->setData(1, Qt::UserRole, static_cast<int>(severity));
    if (!items.empty()) {
        item->setToolTip(2, text + QLatin1Char('\n') + tr("Double-click to show it on the schematic."));
    }
    list_->scrollToItem(item);
    emit counts_changed(warnings_, errors_);
    if (severity == Severity::Error) {
        emit error_added();
    }
}

void MessagesPanel::clear()
{
    list_->clear();
    warnings_ = 0;
    errors_ = 0;
    emit counts_changed(0, 0);
}

void MessagesPanel::clear_source(const QString& source)
{
    for (int i = list_->topLevelItemCount() - 1; i >= 0; --i) {
        auto* item = list_->topLevelItem(i);
        if (item->text(3) == source) {
            const auto severity = static_cast<Severity>(item->data(1, Qt::UserRole).toInt());
            warnings_ -= severity == Severity::Warning ? 1 : 0;
            errors_ -= severity == Severity::Error ? 1 : 0;
            delete item;
        }
    }
    emit counts_changed(warnings_, errors_);
}

int MessagesPanel::count() const
{
    return list_->topLevelItemCount();
}

} // namespace simnodus::ui
