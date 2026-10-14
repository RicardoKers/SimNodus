// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "properties_panel.hpp"

#include "document.hpp"
#include "theme.hpp"

#include "editing/operations.hpp"
#include "library/component_library.hpp"
#include "model/engineering.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace simnodus::ui {
namespace {

QString translated(const std::string& text)
{
    return QCoreApplication::translate("ComponentLibrary", text.c_str());
}

// Empty if valid; otherwise the reason shown on the field.
QString validate(const library::Parameter& p, const QString& text)
{
    if (!p.choices.empty()) {
        return {};
    }
    const auto value = model::parse_engineering(text.toStdString(), p.unit);
    if (!value) {
        return QCoreApplication::translate("PropertiesPanel", "Not a number. Examples: 4k7, 100n, 2.2u, 1meg.");
    }
    if (p.min && *value < *p.min) {
        return QCoreApplication::translate("PropertiesPanel", "Must be at least %1.")
            .arg(QString::fromStdString(model::format_engineering(*p.min) + p.unit));
    }
    if (p.max && *value > *p.max) {
        return QCoreApplication::translate("PropertiesPanel", "Must be at most %1.")
            .arg(QString::fromStdString(model::format_engineering(*p.max) + p.unit));
    }
    return {};
}

void mark(QLineEdit* edit, const QString& problem)
{
    edit->setStyleSheet(problem.isEmpty() ? QString() : QStringLiteral("QLineEdit { border: 1px solid #c0392b; }"));
    edit->setToolTip(problem);
}

} // namespace

PropertiesPanel::PropertiesPanel(Document* document, QWidget* parent) : QWidget(parent), document_(document)
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

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    form_host_ = new QWidget;
    form_host_->setObjectName(QStringLiteral("parameterForm"));
    form_ = new QFormLayout(form_host_);
    form_->setContentsMargins(6, 2, 6, 2);
    form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    scroll->setWidget(form_host_);
    layout->addWidget(scroll, 2);

    layout->addWidget(theme::make_caption(tr("Live values")));
    live_ = new QLabel(tr("Not running."));
    live_->setContentsMargins(6, 2, 6, 6);
    live_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    live_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    live_->setObjectName(QStringLiteral("liveValues"));
    QFont mono = live_->font();
    mono.setFamily(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    live_->setFont(mono);
    layout->addWidget(live_, 1);

    if (document_) {
        // A change is often applied from inside an editor's own signal or
        // focus-out handler; rebuilding then would delete that editor while
        // Qt still uses it. Rebuild once control is back in the event loop.
        connect(document_, &Document::changed, this, &PropertiesPanel::schedule_rebuild);
    }
    clear_selection();
}

void PropertiesPanel::schedule_rebuild()
{
    if (rebuild_pending_) {
        return;
    }
    rebuild_pending_ = true;
    QTimer::singleShot(0, this, [this] {
        rebuild_pending_ = false;
        if (!editing()) {
            rebuild();
        }
    });
}

bool PropertiesPanel::editing() const
{
    const QWidget* focus = QApplication::focusWidget();
    return focus && form_host_->isAncestorOf(focus) && qobject_cast<const QLineEdit*>(focus);
}

void PropertiesPanel::set_selection(const std::set<model::Id>& ids)
{
    selection_ = ids;
    rebuild();
}

void PropertiesPanel::clear_selection()
{
    selection_.clear();
    rebuild();
}

QString PropertiesPanel::header_text() const
{
    return title_->text();
}

void PropertiesPanel::set_live_text(const QString& text)
{
    live_->setText(text.isEmpty() ? tr("Not running.") : text);
}

void PropertiesPanel::focus_value()
{
    if (value_editor_) {
        value_editor_->setFocus();
        if (auto* edit = qobject_cast<QLineEdit*>(value_editor_)) {
            edit->selectAll();
        }
    }
}

void PropertiesPanel::rebuild()
{
    rebuilding_ = true;
    value_editor_ = nullptr;
    // Old editors may still be inside one of their own event handlers:
    // detach them now and delete them later.
    while (form_->rowCount() > 0) {
        const auto row = form_->takeRow(0);
        for (QLayoutItem* item : {row.labelItem, row.fieldItem}) {
            if (item && item->widget()) {
                QWidget* widget = item->widget();
                widget->hide();
                widget->setParent(nullptr);
                widget->deleteLater();
            }
            delete item;
        }
    }
    const model::Schematic* s = document_ ? &document_->schematic() : nullptr;
    std::vector<const model::Component*> components;
    std::vector<const model::NetLabel*> labels;
    int others = 0;
    for (model::Id id : selection_) {
        if (!s) {
            break;
        }
        if (const auto* c = s->component(id)) {
            components.push_back(c);
        } else if (const auto* l = s->label(id)) {
            labels.push_back(l);
        } else if (s->contains(id)) {
            ++others;
        }
    }
    if (selection_.empty() || !s) {
        title_->setText(tr("No selection"));
        subtitle_->setText(tr("Select an item on the schematic to edit its properties."));
        rebuilding_ = false;
        return;
    }
    if (components.empty() && labels.size() == 1 && others == 0) {
        title_->setText(tr("Net label"));
        subtitle_->setText(tr("Points with the same label name are connected."));
        auto* name = new QLineEdit(QString::fromStdString(labels.front()->name));
        connect(name, &QLineEdit::editingFinished, this, [this, name] { commit_label(name->text()); });
        form_->addRow(tr("Name"), name);
        value_editor_ = name;
        rebuilding_ = false;
        return;
    }
    if (components.empty()) {
        title_->setText(
            selection_.size() == 1 ? tr("Wire or probe") : tr("%n items", nullptr, static_cast<int>(selection_.size())));
        subtitle_->setText(tr("Drag to move; Del deletes."));
        rebuilding_ = false;
        return;
    }
    // Components: parameters are edited for one type at a time.
    const std::string type = components.front()->type;
    const bool same_type =
        std::all_of(components.begin(), components.end(), [&](const model::Component* c) { return c->type == type; });
    const auto* d = document_->library().find(type);
    if (components.size() == 1) {
        title_->setText(QString::fromStdString(components.front()->designator));
        subtitle_->setText(d ? translated(d->name) : tr("Unknown type %1").arg(QString::fromStdString(type)));
        auto* designator = new QLineEdit(QString::fromStdString(components.front()->designator));
        designator->setObjectName(QStringLiteral("designator"));
        connect(designator, &QLineEdit::editingFinished, this,
            [this, designator] { commit_designator(designator->text()); });
        form_->addRow(tr("Name"), designator);
    } else {
        title_->setText(tr("%n components", nullptr, static_cast<int>(components.size())));
        subtitle_->setText(same_type && d ? tr("Common parameters of %1").arg(translated(d->name))
                                          : tr("Select components of one type to edit them together."));
    }
    if (!same_type || !d) {
        rebuilding_ = false;
        return;
    }
    for (const auto& p : d->parameters) {
        // The common value, or empty when the selected components differ.
        QString value;
        bool common = true;
        for (std::size_t i = 0; i < components.size(); ++i) {
            const auto it = components[i]->parameters.find(p.name);
            const QString v =
                QString::fromStdString(it == components[i]->parameters.end() ? p.default_value : it->second);
            if (i == 0) {
                value = v;
            } else if (v != value) {
                common = false;
            }
        }
        QString label = translated(p.label);
        if (!p.unit.empty()) {
            label += QStringLiteral(" (") + QString::fromStdString(p.unit == "Ohm" ? std::string("\xCE\xA9") : p.unit)
                + QStringLiteral(")");
        }
        QWidget* editor = nullptr;
        if (!p.choices.empty()) {
            auto* box = new QComboBox;
            for (const auto& choice : p.choices) {
                box->addItem(translated(choice), QString::fromStdString(choice));
            }
            box->setCurrentIndex(common ? box->findData(value) : -1);
            const std::string name = p.name;
            connect(box, &QComboBox::activated, this,
                [this, box, name](int) { commit_parameter(name, box->currentData().toString()); });
            editor = box;
        } else {
            auto* edit = new QLineEdit(common ? value : QString());
            edit->setPlaceholderText(common ? QString() : tr("(different values)"));
            edit->setToolTip(translated(p.description));
            const library::Parameter parameter = p;
            connect(edit, &QLineEdit::textEdited, this,
                [edit, parameter](const QString& text) { mark(edit, validate(parameter, text)); });
            connect(edit, &QLineEdit::editingFinished, this, [this, edit, parameter] {
                if (rebuilding_ || edit->text().isEmpty()) {
                    return;
                }
                const QString problem = validate(parameter, edit->text());
                mark(edit, problem);
                if (problem.isEmpty()) {
                    commit_parameter(parameter.name, edit->text().trimmed());
                } else {
                    emit message(tr("%1: %2").arg(translated(parameter.label), problem));
                }
            });
            editor = edit;
        }
        editor->setObjectName(QStringLiteral("parameter_%1").arg(QString::fromStdString(p.name)));
        form_->addRow(label, editor);
        if (!value_editor_ || p.name == d->value_parameter) {
            value_editor_ = editor;
        }
    }
    rebuilding_ = false;
}

void PropertiesPanel::commit_parameter(const std::string& name, const QString& text)
{
    if (!document_) {
        return;
    }
    const auto& s = document_->schematic();
    model::Schematic work = s;
    for (model::Id id : selection_) {
        if (const auto* c = work.component(id)) {
            const auto it = c->parameters.find(name);
            if (it != c->parameters.end() && it->second == text.toStdString()) {
                continue;
            }
            editing::set_parameter(work, id, name, text.toStdString()).apply(work);
        }
    }
    const std::string what = selection_.size() == 1 && s.component(*selection_.begin())
        ? s.component(*selection_.begin())->designator
        : std::string("components");
    document_->apply(model::make_edit(s, work, "Set " + what + " " + name));
}

void PropertiesPanel::commit_designator(const QString& text)
{
    if (!document_ || selection_.size() != 1 || text.trimmed().isEmpty()) {
        return;
    }
    const auto& s = document_->schematic();
    const model::Id id = *selection_.begin();
    const std::string designator = text.trimmed().toStdString();
    if (const auto* other = s.component_by_designator(designator); other && other->id != id) {
        emit message(tr("The name %1 is already used.").arg(text.trimmed()));
        schedule_rebuild();
        return;
    }
    document_->apply(editing::set_designator(s, id, designator));
}

void PropertiesPanel::commit_label(const QString& text)
{
    if (!document_ || selection_.size() != 1 || text.trimmed().isEmpty()) {
        return;
    }
    const auto& s = document_->schematic();
    const auto* l = s.label(*selection_.begin());
    if (!l || l->name == text.trimmed().toStdString()) {
        return;
    }
    model::Schematic work = s;
    model::NetLabel renamed = *l;
    renamed.name = text.trimmed().toStdString();
    work.put(renamed);
    document_->apply(model::make_edit(s, work, "Rename net label"));
}

} // namespace simnodus::ui
