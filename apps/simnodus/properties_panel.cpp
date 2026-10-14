// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "properties_panel.hpp"

#include "theme.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace simnodus::ui {

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("propertiesPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(theme::make_caption(tr("Properties")));

    title_ = new QLabel;
    QFont bold = title_->font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 1.1);
    title_->setFont(bold);
    title_->setContentsMargins(6, 2, 6, 0);
    subtitle_ = new QLabel;
    subtitle_->setContentsMargins(6, 0, 6, 4);
    subtitle_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(subtitle_);

    parameters_ = new QTableWidget(0, 3);
    parameters_->setHorizontalHeaderLabels({tr("Parameter"), tr("Value"), tr("Unit")});
    parameters_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    parameters_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    parameters_->verticalHeader()->hide();
    parameters_->setFrameShape(QFrame::NoFrame);
    parameters_->setObjectName(QStringLiteral("parameterTable"));
    layout->addWidget(parameters_, 2);

    layout->addWidget(theme::make_caption(tr("Live values")));
    live_ = new QLabel(tr("Not running."));
    live_->setContentsMargins(6, 2, 6, 6);
    live_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    live_->setObjectName(QStringLiteral("liveValues"));
    layout->addWidget(live_, 1);

    clear_selection();
}

void PropertiesPanel::show_type(const QString& name)
{
    title_->setText(name);
    subtitle_->setText(tr("Placing and editing components arrive in the next builds."));
}

void PropertiesPanel::clear_selection()
{
    title_->setText(tr("No selection"));
    subtitle_->setText(tr("Select an item on the schematic to edit its properties."));
}

QString PropertiesPanel::header_text() const
{
    return title_->text();
}

} // namespace simnodus::ui
