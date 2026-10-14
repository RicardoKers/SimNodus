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
}

void MessagesPanel::add(Severity severity, const QString& text, const QString& source)
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

int MessagesPanel::count() const
{
    return list_->topLevelItemCount();
}

} // namespace simnodus::ui
