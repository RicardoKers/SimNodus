// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "properties_panel.hpp"

#include "document.hpp"
#include "theme.hpp"
#include "user_models.hpp"

#include "editing/operations.hpp"
#include "library/component_library.hpp"
#include "model/engineering.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <sstream>

namespace simnodus::ui {
namespace {

QString translated(const std::string& text)
{
    return QCoreApplication::translate("ComponentLibrary", text.c_str());
}

// Empty if valid; otherwise the reason shown on the field.
QString validate(const library::Parameter& p, const QString& text)
{
    if (!p.choices.empty() || p.kind == library::Parameter::Kind::File) {
        return {};
    }
    if (p.kind == library::Parameter::Kind::Text) {
        return text.trimmed().isEmpty() ? QCoreApplication::translate("PropertiesPanel", "Enter a name.") : QString();
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
    std::vector<const model::TextNote*> notes;
    int others = 0;
    for (model::Id id : selection_) {
        if (!s) {
            break;
        }
        if (const auto* c = s->component(id)) {
            components.push_back(c);
        } else if (const auto* l = s->label(id)) {
            labels.push_back(l);
        } else if (const auto* n = s->note(id)) {
            notes.push_back(n);
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
    if (selection_.size() == 1) {
        const model::Id id = *selection_.begin();
        if (const auto* b = s->bus(id)) {
            title_->setText(tr("Bus"));
            subtitle_->setText(tr("Bit k is the net named after the bus and k; splitters on the bus follow a new name."));
            auto* name = new QLineEdit(QString::fromStdString(b->name));
            name->setObjectName(QStringLiteral("busName"));
            connect(name, &QLineEdit::editingFinished, this, [this, name, id] {
                if (document_ && !rebuilding_ && !name->text().trimmed().isEmpty()) {
                    document_->apply(editing::rename_bus(document_->schematic(), id, name->text().trimmed().toStdString()));
                }
            });
            form_->addRow(tr("Name"), name);
            value_editor_ = name;
            rebuilding_ = false;
            return;
        }
        if (const auto* sp = s->splitter(id)) {
            title_->setText(tr("Splitter"));
            subtitle_->setText(tr("Its bit pins act as net labels: %1%2 ... %1%3.")
                                   .arg(QString::fromStdString(sp->bus))
                                   .arg(sp->first)
                                   .arg(sp->first + sp->count - 1));
            auto* bus = new QLineEdit(QString::fromStdString(sp->bus));
            bus->setObjectName(QStringLiteral("splitterBus"));
            auto* first = new QSpinBox;
            first->setObjectName(QStringLiteral("splitterFirst"));
            first->setRange(0, 63);
            first->setValue(sp->first);
            auto* count = new QSpinBox;
            count->setObjectName(QStringLiteral("splitterCount"));
            count->setRange(1, 64);
            count->setValue(sp->count);
            auto commit = [this, id, bus, first, count] {
                if (document_ && !rebuilding_ && !bus->text().trimmed().isEmpty()) {
                    document_->apply(editing::set_splitter(
                        document_->schematic(), id, bus->text().trimmed().toStdString(), first->value(), count->value()));
                }
            };
            connect(bus, &QLineEdit::editingFinished, this, commit);
            connect(first, &QSpinBox::editingFinished, this, commit);
            connect(count, &QSpinBox::editingFinished, this, commit);
            form_->addRow(tr("Bus name"), bus);
            form_->addRow(tr("First bit"), first);
            form_->addRow(tr("Number of bits"), count);
            value_editor_ = bus;
            rebuilding_ = false;
            return;
        }
    }
    if (components.empty() && labels.empty() && notes.size() == 1 && others == 0) {
        title_->setText(tr("Text note"));
        subtitle_->setText(tr("Presentation only; it does not change the circuit."));
        auto* text = new QPlainTextEdit(QString::fromStdString(notes.front()->text));
        text->setObjectName(QStringLiteral("noteText"));
        text->setTabChangesFocus(true);
        text->setMinimumHeight(90);
        auto* apply = new QPushButton(tr("Apply"));
        apply->setObjectName(QStringLiteral("noteApply"));
        connect(apply, &QPushButton::clicked, this, [this, text] {
            if (document_ && selection_.size() == 1) {
                document_->apply(editing::set_note_text(
                    document_->schematic(), *selection_.begin(), text->toPlainText().trimmed().isEmpty()
                        ? std::string() : text->toPlainText().toStdString()));
            }
        });
        form_->addRow(tr("Text"), text);
        form_->addRow(QString(), apply);
        value_editor_ = text;
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
        if (p.kind == library::Parameter::Kind::File) {
            editor = file_editor(p, common ? value : QString());
        } else if (p.kind == library::Parameter::Kind::Model) {
            editor = model_editor(p, *components.front(), common ? value : QString());
        } else if (!p.choices.empty()) {
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
    if (d->block && components.size() == 1) {
        // A block (PRJ-06): the project keeps a copy of its file.
        const model::Id id = components.front()->id;
        const auto* block = document_->schematic().block(model::block_name(type));
        auto* file = new QLabel(block && !block->source.empty() ? QString::fromStdString(block->source)
                                                                 : tr("(no file)"));
        file->setWordWrap(true);
        file->setToolTip(tr("The project keeps a copy of this file; Update reads the file again."));
        form_->addRow(tr("Block file"), file);
        auto* buttons = new QWidget;
        auto* row = new QHBoxLayout(buttons);
        row->setContentsMargins(0, 0, 0, 0);
        auto* open = new QPushButton(tr("Open"));
        open->setObjectName(QStringLiteral("openBlock"));
        open->setToolTip(tr("Open the block's file to change it (also with a double-click on the block)"));
        auto* update = new QPushButton(tr("Update"));
        update->setObjectName(QStringLiteral("updateBlock"));
        update->setToolTip(tr("Read the block's file again; every block of this kind in the project changes"));
        row->addWidget(open);
        row->addWidget(update);
        connect(open, &QPushButton::clicked, this, [this, id] { emit block_open_requested(id); });
        connect(update, &QPushButton::clicked, this, [this, id] { emit block_update_requested(id); });
        form_->addRow(QString(), buttons);
    }
    rebuilding_ = false;
}

QWidget* PropertiesPanel::model_editor(
    const library::Parameter& p, const model::Component& first, const QString& value)
{
    // Built-in models of the type, then the user's (PRJ-07); each item keeps the
    // name and, for a model that is not built in, its statement.
    auto* box = new QComboBox;
    box->setEditable(true); // type to search a long list
    box->setInsertPolicy(QComboBox::NoInsert);
    auto add_item = [box](const std::string& name, const QString& statement, const QString& tip) {
        box->addItem(QString::fromStdString(name), QStringList{QString::fromStdString(name), statement});
        box->setItemData(box->count() - 1, tip, Qt::ToolTipRole);
    };
    auto add = [&add_item](const library::SpiceModel& m, const QString& where, const QString& statement) {
        const QString description = QString::fromStdString(m.description);
        add_item(m.name, statement, description + (description.isEmpty() ? QString() : QStringLiteral("\n")) + where);
    };
    // Vendor subcircuits (ADR 0097) fit when they have one port per pin of the part.
    auto add_subcircuit = [&add_item, &p](const library::SpiceSubcircuit& s, const QString& where) {
        if (s.ports.size() != p.subcircuit.size()) {
            return;
        }
        QString ports;
        for (const auto& port : s.ports) {
            ports += QString::fromStdString(port) + QLatin1Char(' ');
        }
        const QString description = QString::fromStdString(s.description);
        add_item(s.name, QString::fromStdString(s.text),
            description + (description.isEmpty() ? QString() : QStringLiteral("\n"))
                + QObject::tr("Subcircuit, ports %1\n%2").arg(ports.trimmed(), where));
    };
    if (p.model_type.empty()) {
        add_item(p.default_value, QString(), tr("SimNodus's built-in model of this part"));
    }
    for (const auto& m : document_->library().models()) {
        if (library::model_fits(p, m)) {
            add(m, tr("Built in (SimNodus approximation)"), QString());
        }
    }
    const auto& folder_models = user_models();
    emit models_read();
    for (const auto& u : folder_models) {
        if (library::model_fits(p, u.model) && box->findText(QString::fromStdString(u.model.name)) < 0) {
            add(u.model, tr("Models folder: %1").arg(u.file), QString::fromStdString(u.model.card()));
        }
    }
    if (!p.subcircuit.empty()) {
        for (const auto& u : user_subcircuits()) {
            if (box->findText(QString::fromStdString(u.subcircuit.name)) < 0) {
                add_subcircuit(u.subcircuit, tr("Models folder: %1").arg(u.file));
            }
        }
    }
    // A model kept in the project but missing here (made on another computer).
    if (const auto kept = first.models.find(p.name); kept != first.models.end() && !value.isEmpty()
        && box->findText(value) < 0) {
        if (const auto m = library::parse_spice_model(kept->second)) {
            add(*m, tr("Kept in the project"), QString::fromStdString(kept->second));
        } else if (const auto s = library::parse_subcircuit_text(kept->second)) {
            add_subcircuit(*s, tr("Kept in the project"));
        }
    }
    auto* completer = new QCompleter(box->model(), box);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    box->setCompleter(completer);
    box->setCurrentIndex(box->findText(value));
    if (box->currentIndex() < 0) {
        box->setEditText(value);
    }
    box->setToolTip(translated(p.description));
    const std::string name = p.name;
    connect(box, &QComboBox::activated, this, [this, box, name](int index) {
        const QStringList data = box->itemData(index).toStringList();
        if (data.size() == 2) {
            commit_model(name, data[0].toStdString(), data[1].toStdString());
        }
    });
    // A vendor subcircuit: which of its ports is which pin of the part.
    const auto kept = first.models.find(p.name);
    const auto sub = kept == first.models.end() ? std::nullopt : library::parse_subcircuit_text(kept->second);
    if (!sub || selection_.size() != 1) {
        return box;
    }
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(box, 1);
    auto* pins = new QPushButton(tr("Pins..."));
    pins->setObjectName(QStringLiteral("modelPins"));
    pins->setToolTip(tr("Which port of the subcircuit is which pin of the part"));
    layout->addWidget(pins);
    const model::Id id = first.id;
    const library::Parameter parameter = p;
    const library::SpiceSubcircuit subcircuit = *sub;
    connect(pins, &QPushButton::clicked, this, [this, id, parameter, subcircuit] { edit_model_pins(id, parameter, subcircuit); });
    return row;
}

std::vector<std::string> PropertiesPanel::model_pins(model::Id component, const library::Parameter& p) const
{
    const auto* c = document_ ? document_->schematic().component(component) : nullptr;
    if (!c) {
        return p.subcircuit;
    }
    const auto it = c->models.find(p.name + ".pins");
    if (it == c->models.end()) {
        return p.subcircuit;
    }
    std::vector<std::string> pins;
    std::stringstream list(it->second);
    for (std::string pin; std::getline(list, pin, ',');) {
        pins.push_back(pin);
    }
    return pins.size() == p.subcircuit.size() ? pins : p.subcircuit;
}

void PropertiesPanel::edit_model_pins(model::Id component, const library::Parameter& p, const library::SpiceSubcircuit& s)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Pins of %1").arg(QString::fromStdString(s.name)));
    auto* layout = new QVBoxLayout(&dialog);
    if (!s.description.empty()) {
        auto* notes = new QLabel(QString::fromStdString(s.description));
        notes->setWordWrap(true);
        notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(notes);
    }
    auto* form = new QFormLayout;
    layout->addLayout(form);
    const auto current = model_pins(component, p);
    std::vector<QComboBox*> boxes;
    for (std::size_t k = 0; k < s.ports.size(); ++k) {
        auto* box = new QComboBox;
        box->setObjectName(QStringLiteral("port_%1").arg(k));
        for (const auto& pin : p.subcircuit) {
            box->addItem(QString::fromStdString(pin));
        }
        box->setCurrentText(QString::fromStdString(current[k]));
        form->addRow(tr("Port %1 (%2)").arg(k + 1).arg(QString::fromStdString(s.ports[k])), box);
        boxes.push_back(box);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [&boxes, &p] {
        for (std::size_t k = 0; k < boxes.size(); ++k) {
            boxes[k]->setCurrentText(QString::fromStdString(p.subcircuit[k]));
        }
    });
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::vector<std::string> chosen;
    for (auto* box : boxes) {
        chosen.push_back(box->currentText().toStdString());
    }
    auto sorted = chosen;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
        emit message(tr("Each pin of the part must be used once."));
        return;
    }
    document_->apply(editing::set_model_pins(
        document_->schematic(), component, p.name, chosen == p.subcircuit ? std::vector<std::string>{} : chosen));
}

void PropertiesPanel::commit_model(const std::string& parameter, const std::string& name, const std::string& statement)
{
    if (!document_) {
        return;
    }
    const auto& s = document_->schematic();
    model::Schematic work = s;
    for (model::Id id : selection_) {
        if (work.component(id)) {
            editing::set_model(work, id, parameter, name, statement).apply(work);
        }
    }
    document_->apply(model::make_edit(s, work, "Set model " + name));
}

QString PropertiesPanel::project_dir() const
{
    return document_ ? document_->base_dir() : QString();
}

QWidget* PropertiesPanel::file_editor(const library::Parameter& p, const QString& value)
{
    auto* host = new QWidget;
    auto* row = new QHBoxLayout(host);
    row->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QLineEdit(value);
    edit->setObjectName(QStringLiteral("file_%1").arg(QString::fromStdString(p.name)));
    edit->setPlaceholderText(tr("Choose a file"));
    const QString dir = project_dir();
    const QString absolute = value.isEmpty() || dir.isEmpty() ? value : QDir(dir).absoluteFilePath(value);
    const QFileInfo info(absolute);
    edit->setToolTip(value.isEmpty() ? translated(p.description)
            : info.isFile() ? tr("%1\n%2 bytes, modified %3")
                                  .arg(QDir::toNativeSeparators(info.absoluteFilePath()))
                                  .arg(info.size())
                                  .arg(QLocale().toString(info.lastModified(), QLocale::ShortFormat))
                            : tr("%1 does not exist").arg(QDir::toNativeSeparators(absolute)));
    mark(edit, value.isEmpty() || info.isFile() ? QString() : edit->toolTip());
    const std::string name = p.name;
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, name] {
        if (!rebuilding_) {
            commit_parameter(name, QDir::fromNativeSeparators(edit->text().trimmed()));
        }
    });
    auto* browse = new QPushButton(tr("Browse..."));
    browse->setObjectName(QStringLiteral("browse_%1").arg(QString::fromStdString(p.name)));
    const QString filter = p.filter.empty() ? tr("All files (*)")
                                            : tr("Firmware (%1);;All files (*)").arg(QString::fromStdString(p.filter));
    connect(browse, &QPushButton::clicked, this, [this, name, filter, absolute] {
        const QString start = !absolute.isEmpty() ? QFileInfo(absolute).absolutePath() : project_dir();
        const QString chosen = QFileDialog::getOpenFileName(this, tr("Choose the firmware"), start, filter);
        if (chosen.isEmpty()) {
            return;
        }
        // Relative to the project when it has a folder, so projects can be moved with their firmware.
        const QString dir = project_dir();
        commit_parameter(name, dir.isEmpty() ? chosen : QDir(dir).relativeFilePath(chosen));
    });
    auto* reload = new QPushButton(tr("Reload"));
    reload->setObjectName(QStringLiteral("reload_%1").arg(QString::fromStdString(p.name)));
    reload->setToolTip(tr("Use the rebuilt file: restarts a running simulation"));
    connect(reload, &QPushButton::clicked, this, &PropertiesPanel::firmware_reload_requested);
    row->addWidget(edit, 1);
    row->addWidget(browse);
    row->addWidget(reload);
    return host;
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
