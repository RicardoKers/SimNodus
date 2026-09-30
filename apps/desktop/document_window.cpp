// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "document_window.hpp"
#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPixmap>
#include <QSplitter>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdio>

namespace {
QString text(std::string_view value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}
std::string utf8(const QString& value) { return value.toUtf8().toStdString(); }
}

DocumentWindow::DocumentWindow()
{
    resize(1100, 650);
    analyzer_.setWindowTitle("SimNodus Signal Analyzer");
    analyzer_.resize(640, 360);
    analyzer_.setCentralWidget(new QLabel("No committed signal data available.\nShared instrumentation is pending."));
    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    project_id_ = new QLabel("No project selected");
    project_id_->setTextFormat(Qt::PlainText);
    layout->addWidget(project_id_);
    layout->addWidget(new QLabel("Project display name"));
    name_ = new QLineEdit;
    name_->setAccessibleName("Project display name");
    name_->setMaxLength(320); // Native validation retains its 80-scalar bound.
    layout->addWidget(name_);
    apply_ = new QPushButton("Apply Name");
    layout->addWidget(apply_);
    layout->addWidget(new QLabel("Declared structure (read only; schematic placement and wiring are pending)"));
    structure_ = new QTreeWidget;
    structure_->setHeaderLabels({"Source entity / stable ID", "Definition / explicit terminals"});
    layout->addWidget(structure_);
    setCentralWidget(central);
    components_ = new QDockWidget("Components / Preview", this);
    components_->setObjectName("components-preview");
    catalog_splitter_ = new QSplitter(Qt::Vertical);
    catalog_ = new QListWidget;
    preview_ = new QLabel("Select a declared component definition.\nSymbol rendering is pending.");
    preview_->setWordWrap(true);
    preview_->setTextFormat(Qt::PlainText);
    catalog_splitter_->addWidget(catalog_);
    catalog_splitter_->addWidget(preview_);
    components_->setWidget(catalog_splitter_);
    addDockWidget(Qt::LeftDockWidgetArea, components_);
    properties_ = new QDockWidget("Instance Properties", this);
    properties_->setObjectName("instance-properties");
    inspector_ = new QLabel("Select an instance in the declared structure.");
    inspector_->setWordWrap(true);
    inspector_->setTextFormat(Qt::PlainText);
    auto* instance_panel = new QWidget;
    auto* instance_layout = new QVBoxLayout(instance_panel);
    instance_layout->addWidget(inspector_);
    instance_layout->addWidget(new QLabel("Declared instance display name"));
    instance_name_ = new QLineEdit;
    instance_name_->setAccessibleName("Declared instance display name");
    instance_name_->setMaxLength(320);
    instance_layout->addWidget(instance_name_);
    apply_instance_ = new QPushButton("Apply Instance Name");
    instance_layout->addWidget(apply_instance_);
    auto* definition_note = new QLabel("Edits the selected circuit definition. Reused occurrences share this label; IDs and connections stay unchanged.");
    definition_note->setWordWrap(true);
    instance_layout->addWidget(definition_note);
    resistance_limits_ = new QLabel("Select an existing literal resistance override.");
    resistance_limits_->setWordWrap(true);
    resistance_limits_->setTextFormat(Qt::PlainText);
    instance_layout->addWidget(resistance_limits_);
    resistance_ = new QLineEdit;
    resistance_->setAccessibleName("Declared literal resistance value");
    resistance_->setMaxLength(64);
    instance_layout->addWidget(resistance_);
    apply_resistance_ = new QPushButton("Apply Resistance Value");
    instance_layout->addWidget(apply_resistance_);
    instance_layout->addStretch();
    properties_->setWidget(instance_panel);
    addDockWidget(Qt::RightDockWidgetArea, properties_);
    auto* file = menuBar()->addMenu("File");
    auto* open = file->addAction("Open...");
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        if(!confirmDiscard()) return;
        const auto path = QFileDialog::getOpenFileName(this, "Open project declaration", {}, "Project declarations (*.json)");
        if(path.isEmpty()) return;
        const QFileInfo selected(path);
        openDocument(selected.absolutePath(), selected.fileName());
    });
    auto* copy = file->addAction("Save Copy...");
    connect(copy, &QAction::triggered, this, [this] {
        if(!document_.graph()) return;
        bool accepted = false;
        const auto leaf = QInputDialog::getText(this, "Save Copy", "New filename in the opened document directory:\n" + text(document_.root()),
            QLineEdit::Normal, "project-copy.json", &accepted);
        if(accepted) saveCopy(leaf);
    });
    auto* quit = file->addAction("Exit");
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto* edit = menuBar()->addMenu("Edit");
    undo_ = edit->addAction("Undo Last Edit (one step)");
    redo_ = edit->addAction("Redo Last Edit (one step)");
    connect(undo_, &QAction::triggered, this, [this] { undoEdit(); });
    connect(redo_, &QAction::triggered, this, [this] { redoEdit(); });
    auto* view = menuBar()->addMenu("View");
    view->addAction(components_->toggleViewAction());
    view->addAction(properties_->toggleViewAction());
    auto* analyzer = view->addAction("Open Signal Analyzer");
    connect(analyzer, &QAction::triggered, this, &DocumentWindow::showAnalyzer);
    connect(apply_, &QPushButton::clicked, this, [this] { applyName(name_->text()); });
    connect(apply_instance_, &QPushButton::clicked, this, [this] { applyInstanceName(instance_name_->text()); });
    connect(apply_resistance_, &QPushButton::clicked, this, [this] { applyResistance(resistance_->text()); });
    connect(catalog_, &QListWidget::currentRowChanged, this, &DocumentWindow::selectCatalog);
    connect(structure_, &QTreeWidget::itemSelectionChanged, this, &DocumentWindow::selectInstance);
    connect(name_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    connect(instance_name_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    connect(resistance_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    status_ = new QLabel("Open one project declaration. Resource access and simulation are unavailable.");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    statusBar()->addWidget(status_, 1);
    refresh();
}

bool DocumentWindow::confirmDiscard()
{
    if(!document_.dirty() && !editsPending()) return true;
    return QMessageBox::question(this, "Discard changes?", "Discard unsaved document changes or pending field text? Save Copy creates a separate file and leaves this document edited.",
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard;
}
void DocumentWindow::closeEvent(QCloseEvent* event)
{
    if(!confirmDiscard()) { event->ignore(); return; }
    analyzer_.close();
    event->accept();
    QApplication::quit();
}
void DocumentWindow::showAnalyzer() { analyzer_.show(); analyzer_.raise(); }
void DocumentWindow::reportError(const char* operation, const char* code, std::size_t offset,
    std::uint32_t system, bool cleanup)
{
    status_->setText(QString("%1 refused: %2 (source byte %3, system %4)%5. Current document retained.")
        .arg(operation, code).arg(static_cast<qulonglong>(offset)).arg(system)
        .arg(cleanup ? "; temporary cleanup failed" : ""));
}
bool DocumentWindow::openDocument(const QString& root, const QString& leaf)
{
    const auto result = document_.open(utf8(root), utf8(leaf));
    if(const auto* error = std::get_if<simnodus::ProjectAcquisitionError>(&result)) {
        reportError("Open", error->code, error->offset, error->system_code); return false;
    }
    refresh();
    status_->setText("Opened inert declaration. Referenced resources and runtime remain unverified.");
    return true;
}
bool DocumentWindow::applyName(const QString& name)
{
    const auto result = document_.rename(utf8(name));
    if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
        reportError("Name edit", error->code, error->offset, 0); return false;
    }
    // A name-only edit must not reset either independent inspection selection.
    name_->setText(text(document_.name()));
    setWindowTitle("SimNodus Circuit Editor - " + text(document_.name()) + (document_.dirty() ? " *" : ""));
    updateHistoryActions();
    status_->setText("Name applied in memory. Use Save Copy explicitly to create a new file.");
    return true;
}
bool DocumentWindow::saveCopy(const QString& leaf)
{
    if(editsPending()) {
        status_->setText("Apply or restore pending name/resistance text before Save Copy.");
        return false;
    }
    const auto result = document_.save_copy(utf8(leaf));
    if(const auto* error = std::get_if<simnodus::ProjectSaveError>(&result)) {
        reportError("Save Copy", error->code, error->offset, error->system_code, error->temporary_cleanup_failed); return false;
    }
    status_->setText("Copy created: " + text(document_.root()) + "/" + leaf + ". Original association and edited state retained. Open the copy explicitly if desired.");
    return true;
}
void DocumentWindow::refresh()
{
    const QSignalBlocker blocked(structure_);
    catalog_->clear(); structure_->clear();
    selected_circuit_.clear(); selected_instance_.clear();
    instance_name_->clear(); instance_name_->setEnabled(false); apply_instance_->setEnabled(false);
    resistance_->clear(); resistance_->setEnabled(false); apply_resistance_->setEnabled(false);
    resistance_limits_->setText("Select an existing literal resistance override.");
    inspector_->setText("Select an instance in the declared structure.");
    preview_->setText("Select a declared component definition. Symbol rendering is pending.");
    name_->setEnabled(bool(document_.graph())); apply_->setEnabled(bool(document_.graph()));
    name_->setText(text(document_.name()));
    setWindowTitle("SimNodus Circuit Editor" + (document_.graph() ? " - " + text(document_.name()) : ""));
    updateHistoryActions();
    if(!document_.graph()) return;
    const auto& graph = *document_.graph();
    project_id_->setText("Project ID: " + text(graph.declaration->project_id));
    for(const auto& component : graph.connectivity.components)
        catalog_->addItem(text(component.identity.name) + " [" + text(component.identity.id) + "]");
    for(const auto& circuit : graph.connectivity.circuits) {
        auto* group = new QTreeWidgetItem(structure_, {"Circuit: " + text(circuit.identity.id), text(circuit.identity.name)});
        for(const auto& instance : circuit.instances) {
            auto* item = new QTreeWidgetItem(group, {"Instance: " + text(instance.identity.id), text(instance.definition)});
            item->setData(0, Qt::UserRole, text(circuit.identity.id));
            item->setData(0, Qt::UserRole + 1, text(instance.identity.id));
        }
        for(const auto& net : circuit.nets) {
            QStringList endpoints;
            for(const auto& terminal : net.terminals) {
                if(const auto* port = std::get_if<simnodus::LocalPortTerminal>(&terminal)) endpoints << "port:" + text(port->port);
                else { const auto& pin = std::get<simnodus::InstanceTerminal>(terminal); endpoints << text(pin.instance) + "/" + text(pin.terminal); }
            }
            new QTreeWidgetItem(group, {"Net: " + text(net.identity.id), endpoints.join(", ")});
        }
    }
    structure_->expandAll(); structure_->resizeColumnToContents(0);
}
void DocumentWindow::selectCatalog(int row)
{
    if(!document_.graph() || row < 0) return;
    const auto& components = document_.graph()->connectivity.components;
    if(static_cast<std::size_t>(row) >= components.size()) return;
    const auto& component = components[static_cast<std::size_t>(row)];
    QStringList pins;
    for(const auto& pin : component.pins) pins << text(pin.identity.id);
    preview_->setText("Declared definition: " + text(component.identity.id) + "\nPins: " + pins.join(", ") + "\nSymbol rendering pending; no resource access.");
}
void DocumentWindow::selectInstance()
{
    const auto* item = structure_->selectedItems().isEmpty() ? nullptr : structure_->currentItem();
    const auto circuit_id = item ? item->data(0, Qt::UserRole).toString() : QString{};
    const auto instance_id = item ? item->data(0, Qt::UserRole + 1).toString() : QString{};
    if(circuit_id == selected_circuit_ && instance_id == selected_instance_) return;
    if((instanceDraftPending() || resistanceDraftPending()) && QMessageBox::question(this, "Discard pending instance fields?",
        "Discard unapplied instance name/resistance text before changing selection?",
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard) {
        const QSignalBlocker blocked(structure_);
        structure_->setCurrentItem(instanceItem(selected_circuit_, selected_instance_));
        return;
    }
    selected_circuit_ = circuit_id;
    selected_instance_ = instance_id;
    updateInstanceProperties();
}
const simnodus::GraphInstance* DocumentWindow::selectedInstance() const
{
    if(!document_.graph()) return nullptr;
    for(const auto& circuit : document_.graph()->connectivity.circuits) {
        if(text(circuit.identity.id) != selected_circuit_) continue;
        for(const auto& instance : circuit.instances) {
            if(text(instance.identity.id) == selected_instance_) return &instance;
        }
    }
    return nullptr;
}
bool DocumentWindow::instanceDraftPending() const
{
    const auto* instance = selectedInstance();
    return instance && instance_name_->text() != text(instance->identity.name);
}
bool DocumentWindow::editsPending() const
{
    return (document_.graph() && name_->text() != text(document_.name())) || instanceDraftPending() || resistanceDraftPending();
}
std::optional<simnodus::LiteralResistanceView> DocumentWindow::selectedResistance() const
{
    if(!document_.graph()) return {};
    return simnodus::literal_resistance(*document_.graph(), utf8(selected_circuit_), utf8(selected_instance_));
}
bool DocumentWindow::resistanceDraftPending() const
{
    const auto value = selectedResistance();
    return value && resistance_->text() != text(value->value);
}
void DocumentWindow::updateHistoryActions()
{
    undo_->setEnabled(document_.can_undo() && !editsPending());
    redo_->setEnabled(document_.can_redo() && !editsPending());
}
bool DocumentWindow::undoEdit() { return restoreEdit(false); }
bool DocumentWindow::redoEdit() { return restoreEdit(true); }
bool DocumentWindow::restoreEdit(bool redo)
{
    if(editsPending()) {
        status_->setText("Apply or restore pending name/resistance text before Undo/Redo.");
        return false;
    }
    const auto result = redo ? document_.redo_edit() : document_.undo_edit();
    if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
        reportError(redo ? "Redo edit" : "Undo edit", error->code, error->offset, 0); return false;
    }
    // Retain view selections. A full refresh would replace them and the Preview.
    name_->setText(text(document_.name()));
    updateInstanceProperties();
    setWindowTitle("SimNodus Circuit Editor - " + text(document_.name()) + (document_.dirty() ? " *" : ""));
    updateHistoryActions();
    status_->setText(redo ? "One edit redone. Save Copy remains explicit." : "One edit undone. Save Copy remains explicit.");
    return true;
}
QTreeWidgetItem* DocumentWindow::instanceItem(const QString& circuit, const QString& instance) const
{
    for(int i = 0; i < structure_->topLevelItemCount(); ++i) {
        auto* group = structure_->topLevelItem(i);
        for(int j = 0; j < group->childCount(); ++j) {
            auto* row = group->child(j);
            if(row->data(0, Qt::UserRole).toString() == circuit &&
                row->data(0, Qt::UserRole + 1).toString() == instance) return row;
        }
    }
    return nullptr;
}
void DocumentWindow::updateInstanceProperties(bool keep_name_draft, bool keep_resistance_draft)
{
    const auto* instance = selectedInstance();
    instance_name_->setEnabled(instance != nullptr); apply_instance_->setEnabled(instance != nullptr);
    if(instance) {
        if(!keep_name_draft) instance_name_->setText(text(instance->identity.name));
        inspector_->setText("Circuit: " + selected_circuit_ + "\nInstance: " + selected_instance_ + "\nName: " + text(instance->identity.name) + "\nDefinition: " + text(instance->definition) + "\nDeclared properties only; runtime unavailable.");
    } else {
        instance_name_->clear(); inspector_->setText("Select an instance in the declared structure.");
    }
    const auto literal = selectedResistance();
    resistance_->setEnabled(bool(literal)); apply_resistance_->setEnabled(bool(literal));
    if(literal) {
        if(!keep_resistance_draft) resistance_->setText(text(literal->value));
        resistance_limits_->setText("Resistance value in " + text(literal->unit) + " (unit fixed).\nDeclared target limits: " +
            text(literal->minimum) + " to " + text(literal->maximum) + " " + text(literal->base_unit) + ".\nSource declaration edit; runtime remains unavailable.");
    } else {
        resistance_->clear(); resistance_limits_->setText("No supported literal resistance override. Forwarded/default bindings are not editable here.");
    }
    updateHistoryActions();
}
bool DocumentWindow::applyInstanceName(const QString& name)
{
    if(!selectedInstance()) { status_->setText("Select a declared instance before editing its name."); return false; }
    const auto result = document_.rename_instance(utf8(selected_circuit_), utf8(selected_instance_), utf8(name));
    if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
        reportError("Instance name", error->code, error->offset, 0); return false;
    }
    updateInstanceProperties(false, true);
    setWindowTitle("SimNodus Circuit Editor - " + text(document_.name()) + (document_.dirty() ? " *" : ""));
    status_->setText("Instance name applied to the source definition. Save Copy explicitly creates a new file.");
    updateHistoryActions();
    return true;
}
bool DocumentWindow::applyResistance(const QString& value)
{
    if(!selectedResistance()) { status_->setText("Select an existing literal resistance override before editing."); return false; }
    const auto result = document_.set_resistance(utf8(selected_circuit_), utf8(selected_instance_), utf8(value));
    if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
        reportError("Resistance edit", error->code, error->offset, 0); return false;
    }
    updateInstanceProperties(true, false);
    setWindowTitle("SimNodus Circuit Editor - " + text(document_.name()) + (document_.dirty() ? " *" : ""));
    updateHistoryActions();
    status_->setText("Resistance value applied to the declaration. Save Copy remains explicit; simulation is unavailable.");
    return true;
}

void DocumentWindow::runAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    showAnalyzer();
    checks["native_windows"] = QApplication::platformName() == "windows";
    checks["independent_windows"] = isWindow() && analyzer_.isWindow() && analyzer_.isVisible();
    checks["inert_open"] = openDocument(root, "original.json");
    if(document_.graph()) {
        const auto original = std::string(document_.bytes());
        const auto id = document_.graph()->declaration->project_id;
        checks["catalog_and_structure"] = catalog_->count() == 2 && structure_->topLevelItemCount() == 2 && project_id_->text().contains(text(id));
        catalog_->setCurrentRow(0);
        const auto preview = preview_->text();
        structure_->setCurrentItem(structure_->topLevelItem(0)->child(0));
        const auto properties = inspector_->text();
        checks["instance_selection"] = properties.contains("Instance:");
        catalog_->setCurrentRow(1);
        checks["separate_preview_properties"] = preview != preview_->text() && properties == inspector_->text();
        checks["untrusted_labels_plain_text"] = inspector_->textFormat() == Qt::PlainText &&
            preview_->textFormat() == Qt::PlainText && status_->textFormat() == Qt::PlainText;
        name_->setText(QString::fromUtf8("Edited \"RC\" \\ \xCE\xA9"));
        apply_->click();
        checks["name_button_edit"] = document_.dirty() && document_.name() == utf8(name_->text()) &&
            document_.graph()->declaration->project_id == id && document_.bytes() != original;
        checks["edit_retains_selections"] = inspector_->text() == properties;
        const auto revised = document_.graph();
        checks["invalid_name_retains_edit"] = !applyName("") && document_.graph() == revised && document_.dirty();
        checks["failed_open_retains_edit"] = !openDocument(root, "invalid.json") && document_.graph() == revised && document_.dirty();
        checks["explicit_save_copy"] = saveCopy("gui-copy.json") && document_.dirty() && document_.leaf() == "original.json";
        name_->setText("Pending name");
        checks["pending_name_copy_refused"] = !saveCopy("unapplied.json") && document_.graph() == revised;
        QTimer::singleShot(10, this, [] {
            for(auto* widget : QApplication::topLevelWidgets())
                if(auto* dialog = qobject_cast<QMessageBox*>(widget))
                    if(dialog->isVisible()) dialog->button(QMessageBox::Cancel)->click();
        });
        checks["discard_cancel_retains_edit"] = !confirmDiscard() && document_.graph() == revised && name_->text() == "Pending name";
        name_->setText(text(document_.name()));
        checks["occupied_copy_refused"] = !saveCopy("gui-copy.json") && document_.graph() == revised;
        checks["original_refused"] = !saveCopy("original.json") && document_.graph() == revised;
        checks["invalid_leaf_refused"] = !saveCopy("../escape.json") && document_.graph() == revised;
        simnodus::EditorDocument reopened;
        checks["exact_reopen"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(reopened.open(utf8(root), "gui-copy.json")) && reopened.bytes() == document_.bytes();
        components_->hide(); properties_->hide();
        checks["panels_hide"] = !components_->isVisible() && !properties_->isVisible();
        components_->toggleViewAction()->trigger(); properties_->toggleViewAction()->trigger();
        checks["panels_reopen"] = components_->isVisible() && properties_->isVisible();
        resizeDocks({components_, properties_}, {260, 340}, Qt::Horizontal);
        catalog_splitter_->setSizes({200, 150});
        QApplication::processEvents();
        const auto widths = QList<int>{components_->width(), properties_->width()};
        const auto sizes = catalog_splitter_->sizes();
        const auto state = saveState(1);
        const auto split_state = catalog_splitter_->saveState();
        resizeDocks({components_, properties_}, {360, 400}, Qt::Horizontal);
        catalog_splitter_->setSizes({100, 250});
        QApplication::processEvents();
        checks["adjustable_panels"] = widths != QList<int>{components_->width(), properties_->width()} && sizes != catalog_splitter_->sizes();
        const bool restored = restoreState(state, 1) && catalog_splitter_->restoreState(split_state);
        QApplication::processEvents();
        checks["in_memory_panel_restore"] = restored && widths == QList<int>{components_->width(), properties_->width()} && sizes == catalog_splitter_->sizes();
        analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_retains_document"] = analyzer_.isVisible() && document_.graph() == revised;
    }
    bool passed = checks.size() == 23;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()},
        {"selected_instance_text", inspector_->text()},
        {"platform", QApplication::platformName()}, {"scope", "scripted document/inspection controls; no resource or engine execution"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runInstanceAcceptance(const QString& root, const QString& report)
{
    // Dedicated control evidence; unchanged SN-022/window-layout matrices are
    // reused. Scripted dialog answers do not establish human recovery/usability.
    const auto answer = [this](QMessageBox::StandardButton button) {
        QTimer::singleShot(10, this, [button] {
            for(auto* widget : QApplication::topLevelWidgets())
                if(auto* dialog = qobject_cast<QMessageBox*>(widget))
                    if(dialog->isVisible()) dialog->button(button)->click();
        });
    };
    QJsonObject checks;
    checks["native_inert_open"] = QApplication::platformName() == "windows" && openDocument(root, "original.json");
    if(document_.graph()) {
        checks["no_selection_refused"] = !instance_name_->isEnabled() && !applyInstanceName("unselected");
        structure_->setCurrentItem(instanceItem("rc", "r"));
        checks["stable_id_selection"] = selected_circuit_ == "rc" && selected_instance_ == "r" && instance_name_->isEnabled();
        catalog_->setCurrentRow(0);
        const auto preview = preview_->text();
        const auto label = QString::fromUtf8("Edited \"R\" \\ \xCE\xA9");
        instance_name_->setText(label); apply_instance_->click();
        checks["instance_button_edit"] = document_.dirty() && selectedInstance() && text(selectedInstance()->identity.name) == label;
        checks["selection_and_preview_retained"] = selected_circuit_ == "rc" && selected_instance_ == "r" && preview_->text() == preview;
        const auto edited = document_.graph();
        instance_name_->clear(); apply_instance_->click();
        checks["invalid_edit_retains_graph_and_draft"] = document_.graph() == edited && instanceDraftPending() && instance_name_->text().isEmpty();
        catalog_->setCurrentRow(1);
        checks["catalog_preserves_draft"] = instanceDraftPending() && instance_name_->text().isEmpty();
        checks["project_name_apply_preserves_draft"] = applyName(text(document_.name())) && instanceDraftPending() && instance_name_->text().isEmpty();
        const auto before = document_.graph();
        checks["failed_open_preserves_draft"] = !openDocument(root, "invalid.json") && document_.graph() == before && instanceDraftPending();
        checks["pending_copy_refused"] = !saveCopy("unapplied.json") && document_.graph() == before;
        answer(QMessageBox::Cancel);
        structure_->setCurrentItem(instanceItem("main", "left"));
        checks["selection_cancel_preserves_ids_and_draft"] = selected_circuit_ == "rc" && selected_instance_ == "r" &&
            structure_->currentItem() == instanceItem("rc", "r") && instanceDraftPending() && instance_name_->text().isEmpty();
        answer(QMessageBox::Cancel);
        checks["open_guard_cancel_preserves_draft"] = !confirmDiscard() && document_.graph() == before && instanceDraftPending();
        answer(QMessageBox::Cancel);
        close();
        checks["exit_cancel_preserves_document_and_draft"] = isVisible() && document_.graph() == before && instanceDraftPending();
        answer(QMessageBox::Discard);
        structure_->setCurrentItem(instanceItem("main", "left"));
        checks["explicit_draft_discard_changes_selection_only"] = selected_circuit_ == "main" && selected_instance_ == "left" &&
            !instanceDraftPending() && document_.graph() == before;
        structure_->setCurrentItem(structure_->topLevelItem(0));
        checks["circuit_row_refused"] = !instance_name_->isEnabled() && !applyInstanceName("circuit row") && document_.graph() == before;
        auto* circuit = structure_->topLevelItem(0);
        QTreeWidgetItem* net = nullptr;
        for(int i = 0; i < circuit->childCount(); ++i)
            if(circuit->child(i)->text(0).startsWith("Net:")) { net = circuit->child(i); break; }
        structure_->setCurrentItem(net);
        checks["net_row_refused"] = net && !instance_name_->isEnabled() && !applyInstanceName("net row") && document_.graph() == before;
        structure_->setCurrentItem(instanceItem("rc", "r"));
        const auto literal = QString("<img src=\"file:///C:/never-open.png\">");
        checks["untrusted_label_literal"] = applyInstanceName(literal) && inspector_->textFormat() == Qt::PlainText && inspector_->text().contains(literal);
        checks["final_label_edit"] = applyInstanceName(label) && !instanceDraftPending();
        const auto final = document_.graph();
        checks["explicit_copy_retains_association"] = saveCopy("instance-copy.json") && document_.graph() == final && document_.dirty() && document_.leaf() == "original.json";
        simnodus::EditorDocument reopened;
        checks["exact_reopen"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(reopened.open(utf8(root), "instance-copy.json")) && reopened.bytes() == document_.bytes();
        showAnalyzer(); analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_retains_document"] = document_.graph() == final && analyzer_.isVisible();
    }
    bool passed = checks.size() == 21;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()},
        {"platform", QApplication::platformName()}, {"selected_circuit", selected_circuit_},
        {"selected_instance", selected_instance_}, {"instance_properties_text", inspector_->text()},
        {"scope", "scripted declared-instance name edit and draft retention; no engine or resource execution"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runHistoryAcceptance(const QString& root, const QString& report)
{
    // Focused name-history controls only; historical window/backend matrices
    // remain unchanged. Action triggers do not establish human usability.
    QJsonObject checks;
    checks["empty_history_refused"] = !undo_->isEnabled() && !redo_->isEnabled() && !undoEdit() && !redoEdit();
    checks["native_inert_open_clears_history"] = QApplication::platformName() == "windows" && openDocument(root, "original.json") &&
        !document_.can_undo() && !document_.can_redo();
    if(document_.graph()) {
        structure_->setCurrentItem(instanceItem("rc", "r"));
        catalog_->setCurrentRow(0);
        const auto preview = preview_->text();
        const auto original = document_.graph();
        const auto original_name = text(document_.name());
        const auto original_label = instance_name_->text();
        const auto project_name = QString::fromUtf8("History \"project\" \\ \xCE\xA9");
        const auto instance_name = QString::fromUtf8("History \"instance\" \\ \xCE\xA9");
        name_->setText(project_name); apply_->click();
        const auto project = document_.graph();
        checks["project_button_enables_undo"] = document_.dirty() && undo_->isEnabled() && !redo_->isEnabled() && name_->text() == project_name;
        name_->setText("pending project");
        checks["project_draft_refuses_history_and_copy"] = !undo_->isEnabled() && !redo_->isEnabled() && !undoEdit() && !redoEdit() &&
            !saveCopy("blocked.json") && document_.graph() == project && name_->text() == "pending project";
        name_->setText(project_name);
        checks["restored_project_draft_enables_undo"] = undo_->isEnabled() && !editsPending();
        undo_->trigger();
        checks["first_undo_restores_clean_and_view"] = document_.graph() == original && !document_.dirty() && !windowTitle().endsWith(" *") &&
            name_->text() == original_name && selected_circuit_ == "rc" && selected_instance_ == "r" && preview_->text() == preview && redo_->isEnabled();
        instance_name_->clear();
        checks["instance_draft_refuses_history"] = !undo_->isEnabled() && !redo_->isEnabled() && !undoEdit() && !redoEdit() &&
            document_.graph() == original && instance_name_->text().isEmpty();
        apply_instance_->click();
        checks["invalid_instance_preserves_redo_and_draft"] = document_.graph() == original && document_.can_redo() && instanceDraftPending();
        instance_name_->setText(original_label);
        checks["restored_instance_draft_enables_redo"] = redo_->isEnabled() && !editsPending();
        checks["failed_open_preserves_redo"] = !openDocument(root, "invalid.json") && document_.graph() == original && redo_->isEnabled();
        redo_->trigger();
        checks["project_redo_restores_dirty_title_and_fields"] = document_.graph() == project && document_.dirty() && windowTitle().endsWith(" *") &&
            name_->text() == project_name && instance_name_->text() == original_label && undo_->isEnabled() && !redo_->isEnabled();
        instance_name_->setText(instance_name); apply_instance_->click();
        const auto both = document_.graph();
        checks["instance_button_replaces_one_step"] = both != project && instance_name_->text() == instance_name && undo_->isEnabled();
        checks["explicit_combined_copy_retains_history"] = saveCopy("history-both.json") && document_.graph() == both &&
            document_.can_undo() && document_.dirty() && document_.leaf() == "original.json";
        structure_->setCurrentItem(instanceItem("rc", "c"));
        const auto other_label = instance_name_->text();
        undo_->trigger();
        checks["undo_other_instance_keeps_current_selection"] = document_.graph() == project && document_.dirty() &&
            selected_circuit_ == "rc" && selected_instance_ == "c" && instance_name_->text() == other_label && preview_->text() == preview;
        checks["second_undo_refused"] = !undo_->isEnabled() && !undoEdit() && document_.graph() == project && redo_->isEnabled();
        checks["semantic_noop_preserves_redo"] = applyName(project_name) && document_.graph() == project && redo_->isEnabled();
        redo_->trigger();
        checks["redo_other_instance_keeps_current_selection"] = document_.graph() == both && selected_instance_ == "c" &&
            instance_name_->text() == other_label && preview_->text() == preview;
        structure_->setCurrentItem(instanceItem("rc", "r"));
        bool toggles = true;
        for(int i = 0; i < 3; ++i) {
            toggles = toggles && undoEdit() && document_.graph() == project && instance_name_->text() == original_label;
            toggles = toggles && redoEdit() && document_.graph() == both && instance_name_->text() == instance_name;
        }
        checks["repeated_restore_resolves_selected_ids"] = toggles && selected_circuit_ == "rc" && selected_instance_ == "r" && preview_->text() == preview;
        undo_->trigger(); name_->setText("History branch"); apply_->click();
        checks["new_effective_edit_replaces_redo"] = !redo_->isEnabled() && undo_->isEnabled() && name_->text() == "History branch" && instance_name_->text() == original_label;
        undo_->trigger();
        checks["branch_undo_restores_prior_project"] = document_.graph() == project && name_->text() == project_name && redo_->isEnabled();
        checks["copy_of_undone_state_retains_redo"] = saveCopy("history-undone.json") && document_.graph() == project && redo_->isEnabled() && document_.dirty();
        checks["failed_open_retains_undone_state"] = !openDocument(root, "invalid.json") && document_.graph() == project && redo_->isEnabled() && selected_instance_ == "r";
        checks["open_saved_copy_clears_history"] = openDocument(root, "history-both.json") && !document_.dirty() &&
            !undo_->isEnabled() && !redo_->isEnabled() && selected_instance_.isEmpty();
        const auto opened_copy = document_.graph();
        name_->setText("no-selection edit"); apply_->click(); undo_->trigger();
        checks["history_without_instance_selection"] = document_.graph() == opened_copy && !document_.dirty() &&
            !instance_name_->isEnabled() && selected_instance_.isEmpty() && redo_->isEnabled();
        checks["same_path_open_clears_redo"] = openDocument(root, "history-both.json") && !document_.can_redo() && !document_.can_undo() && !document_.dirty();
        const auto final = document_.graph();
        showAnalyzer(); analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_preserves_document"] = document_.graph() == final && analyzer_.isVisible();
    }
    bool passed = checks.size() == 26;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "scripted one-step applied name history; no keyboard, human recovery, resource or engine acceptance"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runResistanceAcceptance(const QString& root, const QString& report)
{
    // A single focused native control path. Scripted answers are not evidence
    // of human recovery, keyboard/accessibility or unchanged layout matrices.
    const auto answer = [this](QMessageBox::StandardButton button) {
        QTimer::singleShot(10, this, [button] {
            for(auto* widget : QApplication::topLevelWidgets())
                if(auto* dialog = qobject_cast<QMessageBox*>(widget))
                    if(dialog->isVisible()) dialog->button(button)->click();
        });
    };
    QJsonObject checks;
    checks["native_inert_open"] = QApplication::platformName() == "windows" && openDocument(root, "original.json");
    if(document_.graph()) {
        checks["no_selection_refused"] = !resistance_->isEnabled() && !applyResistance("2");
        structure_->setCurrentItem(instanceItem("main", "left"));
        catalog_->setCurrentRow(0);
        const auto preview = preview_->text();
        const auto original = document_.graph();
        const auto original_label = instance_name_->text();
        checks["fixed_unit_and_target_bounds"] = resistance_->isEnabled() && resistance_->text() == "1" &&
            resistance_limits_->text().contains("kohm (unit fixed)") && resistance_limits_->text().contains("100 to 10000 ohm");
        resistance_->setText("10.0000001"); apply_resistance_->click();
        checks["outside_bounds_retains_graph_and_draft"] = document_.graph() == original && !document_.dirty() && resistance_->text() == "10.0000001";
        checks["resistance_draft_blocks_copy_and_history"] = !saveCopy("blocked.json") && !undoEdit() && !redoEdit() &&
            !undo_->isEnabled() && !redo_->isEnabled() && document_.graph() == original;
        answer(QMessageBox::Cancel);
        checks["open_cancel_retains_resistance_draft"] = !confirmDiscard() && resistanceDraftPending() && document_.graph() == original;
        answer(QMessageBox::Cancel); close();
        checks["exit_cancel_retains_resistance_draft"] = isVisible() && resistanceDraftPending() && document_.graph() == original;
        resistance_->setText("NaN"); apply_resistance_->click();
        checks["invalid_quantity_retains_graph_and_draft"] = document_.graph() == original && resistance_->text() == "NaN";
        resistance_->setText("3.5"); name_->setText("Resistance project"); instance_name_->setText("Edited left");
        apply_->click();
        checks["project_apply_preserves_both_instance_drafts"] = text(document_.name()) == "Resistance project" &&
            instance_name_->text() == "Edited left" && resistance_->text() == "3.5" && instanceDraftPending() && resistanceDraftPending();
        apply_instance_->click();
        const auto names = document_.graph();
        checks["instance_apply_preserves_resistance_draft"] = text(selectedInstance()->identity.name) == "Edited left" &&
            resistance_->text() == "3.5" && resistanceDraftPending() && name_->text() == "Resistance project";
        name_->setText("pending project"); instance_name_->setText("pending instance");
        apply_resistance_->click();
        const auto both = document_.graph();
        checks["resistance_apply_preserves_both_name_drafts"] = both != names && selectedResistance()->value == "3.5" &&
            name_->text() == "pending project" && instance_name_->text() == "pending instance" && !resistanceDraftPending();
        checks["names_still_block_copy_and_history"] = !saveCopy("blocked.json") && !undoEdit() && !redoEdit() &&
            !undo_->isEnabled() && !redo_->isEnabled() && document_.graph() == both;
        resistance_->setText("4.7");
        answer(QMessageBox::Cancel); structure_->setCurrentItem(instanceItem("main", "right"));
        checks["selection_cancel_retains_all_three_drafts"] = selected_instance_ == "left" && structure_->currentItem() == instanceItem("main", "left") &&
            name_->text() == "pending project" && instance_name_->text() == "pending instance" && resistance_->text() == "4.7" && document_.graph() == both;
        answer(QMessageBox::Discard); structure_->setCurrentItem(instanceItem("main", "right"));
        checks["selection_discard_clears_both_local_drafts"] = selected_instance_ == "right" && !instanceDraftPending() && !resistanceDraftPending() &&
            resistance_->text() == "2.2" && name_->text() == "pending project" && document_.graph() == both;
        name_->setText(text(document_.name()));
        structure_->setCurrentItem(instanceItem("rc", "r"));
        checks["forwarded_binding_refused"] = !resistance_->isEnabled() && !apply_resistance_->isEnabled() && !applyResistance("2") && document_.graph() == both;
        structure_->setCurrentItem(structure_->topLevelItem(0));
        checks["circuit_row_refused"] = !resistance_->isEnabled() && !applyResistance("2") && document_.graph() == both;
        structure_->setCurrentItem(instanceItem("main", "left"));
        checks["selection_and_preview_retained"] = resistance_->text() == "3.5" && instance_name_->text() == "Edited left" &&
            preview_->text() == preview && undo_->isEnabled();
        undo_->trigger();
        checks["mixed_history_undo_keeps_names"] = document_.graph() == names && resistance_->text() == "1" && instance_name_->text() == "Edited left" &&
            name_->text() == "Resistance project" && document_.dirty() && redo_->isEnabled() && !undo_->isEnabled();
        checks["undone_copy_retains_redo"] = saveCopy("resistance-undone.json") && document_.graph() == names && document_.can_redo();
        resistance_->setText("0.01"); apply_resistance_->click();
        checks["invalid_revision_preserves_redo"] = document_.graph() == names && document_.can_redo() && resistanceDraftPending();
        resistance_->setText("1"); apply_resistance_->click();
        checks["no_op_preserves_redo"] = document_.graph() == names && redo_->isEnabled() && !editsPending();
        redo_->trigger();
        checks["redo_restores_resistance_and_names"] = document_.graph() == both && resistance_->text() == "3.5" &&
            instance_name_->text() == "Edited left" && name_->text() == "Resistance project" && windowTitle().endsWith(" *");
        checks["explicit_copy_retains_association_and_history"] = saveCopy("resistance-copy.json") && document_.graph() == both &&
            document_.dirty() && document_.leaf() == "original.json" && document_.can_undo();
        checks["original_and_existing_copy_refused"] = !saveCopy("original.json") && !saveCopy("resistance-copy.json") && document_.graph() == both;
        checks["failed_open_retains_selected_fields"] = !openDocument(root, "invalid.json") && document_.graph() == both &&
            selected_instance_ == "left" && resistance_->text() == "3.5" && instance_name_->text() == "Edited left";
        checks["explicit_reopen_resets_history"] = openDocument(root, "resistance-copy.json") && !document_.dirty() &&
            !document_.can_undo() && !document_.can_redo() && selected_instance_.isEmpty();
        structure_->setCurrentItem(instanceItem("main", "left"));
        checks["reopened_fields_match_persisted_copy"] = resistance_->text() == "3.5" && instance_name_->text() == "Edited left" &&
            name_->text() == "Resistance project" && !editsPending();
        checks["initial_graph_remains_immutable"] = simnodus::literal_resistance(*original, "main", "left")->value == "1" &&
            original_label != "Edited left";
        const auto final = document_.graph();
        showAnalyzer(); analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_preserves_document"] = analyzer_.isVisible() && document_.graph() == final;
    }
    bool passed = checks.size() == 29;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"instance_properties_text", inspector_->text()}, {"resistance_limits_text", resistance_limits_->text()},
        {"scope", "scripted existing literal resistance value and mixed one-step history; no engine, resource or human recovery acceptance"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}
