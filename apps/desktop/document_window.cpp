// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "document_window.hpp"
#include "preview_canvas.hpp"
#include "application/project_inspection.hpp"
#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QImage>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdio>
#include <algorithm>
#include <set>

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
    layout->addWidget(new QLabel("Declared views (read only; schematic placement and wiring are pending)"));
    structure_ = new QTreeWidget;
    structure_->setHeaderLabels({"Source entity / stable ID", "Definition / explicit terminals"});
    views_ = new QTabWidget;
    views_->addTab(structure_, "Structure");
    auto* occurrence_panel = new QWidget;
    auto* occurrence_layout = new QVBoxLayout(occurrence_panel);
    occurrence_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    occurrence_view_choice_ = new QComboBox;
    occurrence_view_choice_->setAccessibleName("Read-only component occurrence path, independent of Properties");
    occurrence_layout->addWidget(occurrence_view_choice_);
    occurrence_artwork_ = new PreviewCanvas;
    occurrence_artwork_->setAccessibleName("Read-only captured artwork for one existing component occurrence");
    occurrence_layout->addWidget(occurrence_artwork_, 1);
    auto* terminals_note = new QLabel("Declared local terminal membership (read only; not flattened electrical connectivity)");
    terminals_note->setWordWrap(true);
    terminals_note->setTextFormat(Qt::PlainText);
    terminals_note->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    occurrence_layout->addWidget(terminals_note);
    occurrence_terminals_ = new QTableWidget(0, 5);
    occurrence_terminals_->setAccessibleName("Declared logical terminals and directly containing local nets");
    occurrence_terminals_->setHorizontalHeaderLabels({"Pin ID", "Pin name", "Local net ID", "Net name", "Local net path"});
    occurrence_terminals_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    occurrence_terminals_->setSelectionBehavior(QAbstractItemView::SelectRows);
    occurrence_terminals_->setSelectionMode(QAbstractItemView::SingleSelection);
    occurrence_terminals_->verticalHeader()->hide();
    occurrence_terminals_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    occurrence_terminals_->horizontalHeader()->setStretchLastSection(true);
    occurrence_terminals_->setMinimumHeight(96);
    occurrence_terminals_->setMaximumHeight(135);
    occurrence_layout->addWidget(occurrence_terminals_);
    local_endpoints_note_ = new QLabel;
    local_endpoints_note_->setWordWrap(true);
    local_endpoints_note_->setTextFormat(Qt::PlainText);
    local_endpoints_note_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    occurrence_layout->addWidget(local_endpoints_note_);
    local_endpoints_ = new QTableWidget(0, 4);
    local_endpoints_->setAccessibleName("All declared endpoints of the selected pin's local net, including the selected pin");
    local_endpoints_->setHorizontalHeaderLabels({"Endpoint kind", "Terminal name", "Definition", "Endpoint ID path"});
    local_endpoints_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    local_endpoints_->setSelectionBehavior(QAbstractItemView::SelectRows);
    local_endpoints_->setSelectionMode(QAbstractItemView::SingleSelection);
    local_endpoints_->verticalHeader()->hide();
    local_endpoints_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    local_endpoints_->horizontalHeader()->setStretchLastSection(true);
    local_endpoints_->setMinimumHeight(110);
    local_endpoints_->setMaximumHeight(150);
    occurrence_layout->addWidget(local_endpoints_);
    inspect_peer_ = new QPushButton("Inspect Selected Component");
    inspect_peer_->setEnabled(false);
    inspect_peer_->setAccessibleName("Inspect a different component from its declared local endpoint; ports are not traversed");
    occurrence_layout->addWidget(inspect_peer_);
    occurrence_view_note_ = new QLabel;
    occurrence_view_note_->setWordWrap(true);
    occurrence_view_note_->setTextFormat(Qt::PlainText);
    occurrence_view_note_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    occurrence_layout->addWidget(occurrence_view_note_);
    capture_occurrence_ = new QPushButton("Capture Occurrence Artwork...");
    occurrence_layout->addWidget(capture_occurrence_);
    views_->addTab(occurrence_panel, "Occurrence (read only)");
    layout->addWidget(views_, 1);
    setCentralWidget(central);
    components_ = new QDockWidget("Components / Preview", this);
    components_->setObjectName("components-preview");
    catalog_splitter_ = new QSplitter(Qt::Vertical);
    catalog_ = new QListWidget;
    preview_ = new QLabel("Select a declared component definition.\nSymbol rendering is pending.");
    preview_->setWordWrap(true);
    preview_->setTextFormat(Qt::PlainText);
    catalog_splitter_->addWidget(catalog_);
    auto* preview_panel = new QWidget;
    auto* preview_layout = new QVBoxLayout(preview_panel);
    preview_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    preview_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    preview_layout->addWidget(preview_);
    artwork_ = new PreviewCanvas;
    preview_layout->addWidget(artwork_, 1);
    artwork_note_ = new QLabel("No captured artwork. Preview requires an explicit resource root.");
    artwork_note_->setWordWrap(true);
    artwork_note_->setTextFormat(Qt::PlainText);
    artwork_note_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    preview_layout->addWidget(artwork_note_);
    preview_artwork_ = new QPushButton("Preview Fixture Artwork...");
    preview_artwork_->setEnabled(false);
    preview_layout->addWidget(preview_artwork_);
    catalog_splitter_->addWidget(preview_panel);
    components_->setWidget(catalog_splitter_);
    addDockWidget(Qt::LeftDockWidgetArea, components_);
    properties_ = new QDockWidget("Instance Properties", this);
    properties_->setObjectName("instance-properties");
    inspector_ = new QLabel("Select an instance in the declared structure.");
    inspector_->setWordWrap(true);
    inspector_->setTextFormat(Qt::PlainText);
    auto* instance_panel = new QWidget;
    auto* instance_layout = new QVBoxLayout(instance_panel);
    instance_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    instance_layout->addWidget(inspector_);
    occurrence_ = new QComboBox;
    occurrence_->setAccessibleName("Resolved declaration occurrence");
    instance_layout->addWidget(occurrence_);
    occurrence_note_ = new QLabel;
    occurrence_note_->setWordWrap(true);
    occurrence_note_->setTextFormat(Qt::PlainText);
    instance_layout->addWidget(occurrence_note_);
    effective_parameters_ = new QTableWidget(0, 3);
    effective_parameters_->setHorizontalHeaderLabels({"Parameter", "Base value / unit", "Binding origin"});
    effective_parameters_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    effective_parameters_->setSelectionMode(QAbstractItemView::NoSelection);
    effective_parameters_->verticalHeader()->hide();
    effective_parameters_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    effective_parameters_->setFixedHeight(105);
    effective_parameters_->setAccessibleName("Applied resolved R/C declaration values");
    instance_layout->addWidget(effective_parameters_);
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
    capacitance_limits_ = new QLabel("Select an existing literal capacitance override.");
    capacitance_limits_->setWordWrap(true);
    capacitance_limits_->setTextFormat(Qt::PlainText);
    instance_layout->addWidget(capacitance_limits_);
    capacitance_ = new QLineEdit;
    capacitance_->setAccessibleName("Declared literal capacitance value");
    capacitance_->setMaxLength(64);
    instance_layout->addWidget(capacitance_);
    apply_capacitance_ = new QPushButton("Apply Capacitance Value");
    instance_layout->addWidget(apply_capacitance_);
    for(auto* label : {inspector_, occurrence_note_, definition_note, resistance_limits_, capacitance_limits_})
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    instance_layout->addStretch();
    properties_scroll_ = new QScrollArea;
    properties_scroll_->setWidgetResizable(true);
    properties_scroll_->setWidget(instance_panel);
    properties_->setWidget(properties_scroll_);
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
    connect(apply_capacitance_, &QPushButton::clicked, this, [this] { applyCapacitance(capacitance_->text()); });
    connect(catalog_, &QListWidget::currentRowChanged, this, &DocumentWindow::selectCatalog);
    connect(preview_artwork_, &QPushButton::clicked, this, [this] {
        const auto root = QFileDialog::getExistingDirectory(this, "Choose explicit symbol resource root");
        if(root.isEmpty()) {
            updateArtworkNote(artwork_->capture() ? "Cancelled; prior captured artwork retained." : "Preview cancelled.");
            return;
        }
        previewArtwork(root);
    });
    connect(structure_, &QTreeWidget::itemSelectionChanged, this, &DocumentWindow::selectInstance);
    connect(name_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    connect(instance_name_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    connect(resistance_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    connect(capacitance_, &QLineEdit::textChanged, this, &DocumentWindow::updateHistoryActions);
    connect(occurrence_, &QComboBox::currentIndexChanged, this, [this] { selectOccurrence(); });
    connect(occurrence_view_choice_, &QComboBox::currentIndexChanged, this, [this] { updateOccurrenceView(); });
    connect(occurrence_terminals_, &QTableWidget::itemSelectionChanged, this, [this] {
        const auto rows = occurrence_terminals_->selectedItems();
        const auto* id = rows.isEmpty() ? nullptr : occurrence_terminals_->item(rows.front()->row(), 0);
        selected_terminal_ = id ? id->data(Qt::UserRole).toString() : QString{};
        updateLocalEndpoints();
    });
    connect(capture_occurrence_, &QPushButton::clicked, this, [this] {
        const auto root = QFileDialog::getExistingDirectory(this, "Choose explicit occurrence artwork resource root");
        if(root.isEmpty()) { updateOccurrenceNote("Capture cancelled; current occurrence retained."); return; }
        captureOccurrence(root);
    });
    connect(local_endpoints_, &QTableWidget::itemSelectionChanged, this, &DocumentWindow::updatePeerAction);
    connect(inspect_peer_, &QPushButton::clicked, this, [this] { inspectSelectedPeer(); });
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
        reportError("Open", error->code, error->offset, error->system_code);
        updateOccurrenceNote(occurrence_capture_ ? "Open refused; prior occurrence capture retained." : "Open refused.");
        return false;
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
    updateParameterInspection();
    retainPreview();
    return true;
}
bool DocumentWindow::saveCopy(const QString& leaf)
{
    if(editsPending()) {
        status_->setText("Apply or restore pending name/R/C text before Save Copy.");
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
    { const QSignalBlocker blocked_choice(occurrence_view_choice_); occurrence_view_choice_->clear(); }
    occurrence_capture_.reset();
    occurrence_artwork_->setCapture({});
    catalog_->clear(); structure_->clear();
    artwork_->setCapture({}); preview_artwork_->setEnabled(false);
    updateArtworkNote();
    selected_circuit_.clear(); selected_instance_.clear();
    instance_name_->clear(); instance_name_->setEnabled(false); apply_instance_->setEnabled(false);
    resistance_->clear(); resistance_->setEnabled(false); apply_resistance_->setEnabled(false);
    resistance_limits_->setText("Select an existing literal resistance override.");
    capacitance_->clear(); capacitance_->setEnabled(false); apply_capacitance_->setEnabled(false);
    capacitance_limits_->setText("Select an existing literal capacitance override.");
    inspector_->setText("Select an instance in the declared structure.");
    preview_->setText("Select a declared component definition. Symbol rendering is pending.");
    name_->setEnabled(bool(document_.graph())); apply_->setEnabled(bool(document_.graph()));
    name_->setText(text(document_.name()));
    setWindowTitle("SimNodus Circuit Editor" + (document_.graph() ? " - " + text(document_.name()) : ""));
    updateHistoryActions();
    updateParameterInspection();
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
    artwork_->setCapture({});
    updateArtworkNote();
    preview_artwork_->setEnabled(false);
    if(!document_.graph() || row < 0) return;
    const auto& components = document_.graph()->connectivity.components;
    if(static_cast<std::size_t>(row) >= components.size()) return;
    const auto& component = components[static_cast<std::size_t>(row)];
    QStringList pins;
    for(const auto& pin : component.pins) pins << text(pin.identity.id);
    preview_->setText("Declared definition: " + text(component.identity.id) + "\nPins: " + pins.join(", ") + "\nGeneral symbol rendering pending. Resource access requires explicit Preview.");
    preview_artwork_->setEnabled(std::holds_alternative<simnodus::SymbolPreviewSelection>(
        simnodus::select_fixture_symbol(*document_.graph(), component.identity.id)));
}
QString DocumentWindow::selectedComponent() const
{
    if(!document_.graph() || catalog_->currentRow() < 0) return {};
    const auto& components = document_.graph()->connectivity.components;
    const auto row = static_cast<std::size_t>(catalog_->currentRow());
    return row < components.size() ? text(components[row].identity.id) : QString{};
}
void DocumentWindow::retainPreview()
{
    const auto& capture = artwork_->capture();
    if(!capture || !document_.graph()) return;
    if(simnodus::fixture_capture_matches(*document_.graph(), utf8(selectedComponent()), *capture)) return;
    artwork_->setCapture({});
    updateArtworkNote("Declared symbol selection changed. Explicit new Preview required.");
}
void DocumentWindow::updateArtworkNote(const QString& message)
{
    artwork_note_->setToolTip({});
    if(const auto& owned = artwork_->capture()) {
        const auto& capture = *owned;
        artwork_note_->setText("Captured fixture artwork for " + text(capture.selection.component) + ".\nResource: " +
            text(capture.selection.request.dependency) + "/" + text(capture.selection.request.resource) +
            "\n227 bytes; SHA-256: " + text(capture.selection.request.sha256.substr(0, 16)) + "...");
        if(capture.pins) {
            QStringList rows;
            for(const auto& pin : *capture.pins) rows << text(pin.logical_pin) + " -> " + text(pin.symbol_pin);
            artwork_note_->setText(artwork_note_->text() + "\nFixture anchor convention: a=left, b=right.\n" + rows.join("; ") +
                "\nDeclared mapping only; model/electrical truth unverified.");
        } else artwork_note_->setText(artwork_note_->text() + "\nPin-anchor correspondence unavailable for this descriptor.");
        artwork_note_->setToolTip("Explicit root: " + text(capture.requested_root) + "\nPath: " + text(capture.selection.request.path) +
            "\nSHA-256: " + text(capture.selection.request.sha256));
    } else artwork_note_->setText("No captured artwork. Preview requires an explicit resource root.");
    if(!message.isEmpty()) artwork_note_->setText(artwork_note_->text() + "\n" + message);
}
bool DocumentWindow::previewArtwork(const QString& resource_root)
{
    if(!document_.graph()) return false;
    const auto result = simnodus::capture_fixture_symbol(*document_.graph(), utf8(selectedComponent()), utf8(resource_root));
    if(const auto* error = std::get_if<simnodus::SymbolPreviewError>(&result)) {
        status_->setText(QString("Preview refused: %1 (system %2). Current document retained.").arg(error->code).arg(error->system_code));
        updateArtworkNote(artwork_->capture() ? "Preview refused; prior captured artwork retained." : "Preview unavailable.");
        return false;
    }
    artwork_->setCapture(std::get<0>(result));
    updateArtworkNote();
    status_->setText("Captured only the selected known symbol. Models, other resources and simulation remain unverified.");
    return true;
}
void DocumentWindow::refreshOccurrenceChoices()
{
    const auto previous = occurrence_view_choice_->currentData().toStringList();
    const QSignalBlocker blocked(occurrence_view_choice_);
    occurrence_view_choice_->clear();
    occurrence_view_choice_->addItem("Select an existing component occurrence", QStringList{});
    if(document_.graph()) {
        std::set<std::string> component_ids;
        for(const auto& component : document_.graph()->connectivity.components) component_ids.insert(component.identity.id);
        std::vector<std::vector<std::string>> paths;
        // The loader already validates every resolved row. List component paths
        // directly; fully cross-check only the currently selected path below.
        for(const auto& row : document_.graph()->declaration->sources.topology.parameters.instances)
            if(component_ids.contains(row.definition)) paths.push_back(row.path);
        std::sort(paths.begin(), paths.end());
        for(const auto& ids : paths) {
            QStringList path;
            for(const auto& id : ids) path << text(id);
            occurrence_view_choice_->addItem(path.join("/"), path);
        }
    }
    const auto retained = occurrence_view_choice_->findData(previous);
    if(retained >= 0) occurrence_view_choice_->setCurrentIndex(retained);
    occurrence_view_choice_->setEnabled(occurrence_view_choice_->count() > 1);
    updateOccurrenceView();
}
void DocumentWindow::updateOccurrenceView()
{
    const auto previous_path = current_occurrence_ ? current_occurrence_->path : std::vector<std::string>{};
    current_occurrence_.reset();
    std::vector<std::string> path;
    for(const auto& id : occurrence_view_choice_->currentData().toStringList()) path.push_back(utf8(id));
    if(document_.graph()) current_occurrence_ = simnodus::inspect_component_occurrence(*document_.graph(), path);
    if(!current_occurrence_ || current_occurrence_->path != previous_path) selected_terminal_.clear();
    const QSignalBlocker terminal_selection(occurrence_terminals_);
    occurrence_terminals_->setRowCount(0);
    int retained_terminal = -1;
    if(current_occurrence_) for(const auto& terminal : current_occurrence_->terminals) {
        QStringList net_path;
        if(terminal.net) for(const auto& id : terminal.net->path) net_path << text(id);
        const QStringList cells{text(terminal.pin), text(terminal.name),
            terminal.net ? text(terminal.net->id) : "Unconnected locally",
            terminal.net ? text(terminal.net->name) : QString{}, net_path.join("/")};
        const auto row = occurrence_terminals_->rowCount();
        occurrence_terminals_->insertRow(row);
        for(int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if(column == 0) item->setData(Qt::UserRole, text(terminal.pin));
            // QTableWidget paints plain text; avoid rich-text tooltips for untrusted labels.
            occurrence_terminals_->setItem(row, column, item);
        }
        if(text(terminal.pin) == selected_terminal_) retained_terminal = row;
    }
    if(retained_terminal >= 0) occurrence_terminals_->setCurrentCell(retained_terminal, 0);
    else selected_terminal_.clear();
    updateLocalEndpoints();
    capture_occurrence_->setEnabled(current_occurrence_ && std::holds_alternative<simnodus::SymbolPreviewSelection>(
        simnodus::select_fixture_symbol(*document_.graph(), current_occurrence_->component)));
    if(occurrence_capture_) {
        const auto& old = occurrence_capture_->occurrence;
        if(!current_occurrence_ || current_occurrence_->path != old.path || current_occurrence_->source_circuit != old.source_circuit ||
            current_occurrence_->source_instance != old.source_instance || current_occurrence_->component != old.component ||
            !simnodus::fixture_capture_matches(*document_.graph(), current_occurrence_->component, *occurrence_capture_->symbol))
            occurrence_capture_.reset();
    }
    occurrence_artwork_->setCapture(occurrence_capture_ ? occurrence_capture_->symbol : nullptr);
    updateOccurrenceNote();
}
void DocumentWindow::updateLocalEndpoints()
{
    const QSignalBlocker endpoint_selection(local_endpoints_);
    inspect_peer_->setEnabled(false);
    current_local_net_.reset();
    local_endpoints_->setRowCount(0);
    local_endpoints_note_->setText("Select a logical pin above to inspect all members of its declared local net.");
    if(!document_.graph() || !current_occurrence_ || selected_terminal_.isEmpty()) return;
    QStringList selected_path;
    for(const auto& id : current_occurrence_->path) selected_path << text(id);
    selected_path << selected_terminal_;
    current_local_net_ = simnodus::inspect_occurrence_local_net(*document_.graph(), current_occurrence_->path, utf8(selected_terminal_));
    if(!current_local_net_) {
        const auto pin = std::find_if(current_occurrence_->terminals.begin(), current_occurrence_->terminals.end(),
            [this](const auto& row) { return text(row.pin) == selected_terminal_; });
        local_endpoints_note_->setText("Selected pin: " + selected_path.join("/") +
            (pin != current_occurrence_->terminals.end() && !pin->net ? "\nUnconnected locally; no declared net members." : "\nLocal net details unavailable."));
        return;
    }
    QStringList net_path;
    for(const auto& id : current_local_net_->net.path) net_path << text(id);
    local_endpoints_note_->setText("Selected pin: " + selected_path.join("/") + "\nLocal net: " + net_path.join("/") +
        "; all declared endpoints below. Ports are not traversed.");
    for(const auto& endpoint : current_local_net_->endpoints) {
        QStringList path;
        for(const auto& id : endpoint.path) path << text(id);
        const auto kind = endpoint.kind == simnodus::DeclaredEndpointKind::local_port ? "Local circuit port" :
            endpoint.kind == simnodus::DeclaredEndpointKind::component_pin ? "Component pin" : "Subcircuit port";
        const QStringList cells{kind, text(endpoint.name), text(endpoint.definition), path.join("/")};
        const auto row = local_endpoints_->rowCount();
        local_endpoints_->insertRow(row);
        for(int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            if(column == 0) {
                item->setData(Qt::UserRole, path);
                item->setData(Qt::UserRole + 1, static_cast<int>(endpoint.kind));
            }
            local_endpoints_->setItem(row, column, item); // Plain cells, no untrusted rich-text tooltips.
        }
    }
}
QStringList DocumentWindow::peerOccurrencePath() const
{
    if(!document_.graph() || !current_occurrence_ || !current_local_net_ || selected_terminal_.isEmpty()) return {};
    const auto rows = local_endpoints_->selectedItems();
    const auto* item = rows.isEmpty() ? nullptr : local_endpoints_->item(rows.front()->row(), 0);
    if(!item || item->data(Qt::UserRole + 1).toInt() != static_cast<int>(simnodus::DeclaredEndpointKind::component_pin)) return {};
    auto path = item->data(Qt::UserRole).toStringList();
    std::vector<std::string> ids;
    for(const auto& id : path) ids.push_back(utf8(id));
    const auto old = std::find_if(current_local_net_->endpoints.begin(), current_local_net_->endpoints.end(),
        [&ids](const auto& row) { return row.kind == simnodus::DeclaredEndpointKind::component_pin && row.path == ids; });
    if(old == current_local_net_->endpoints.end() || ids.size() < 3) return {};
    const auto source = simnodus::inspect_component_occurrence(*document_.graph(), current_occurrence_->path);
    if(!source || source->source_circuit != current_occurrence_->source_circuit ||
        source->source_instance != current_occurrence_->source_instance || source->component != current_occurrence_->component) return {};
    const auto current = simnodus::inspect_occurrence_local_net(*document_.graph(), current_occurrence_->path, utf8(selected_terminal_));
    if(!current || current->net.path != current_local_net_->net.path) return {};
    // Recheck owned identity against the current graph. A stale row grants no navigation.
    const auto member = std::find_if(current->endpoints.begin(), current->endpoints.end(), [&old](const auto& row) {
        return row.kind == old->kind && row.instance == old->instance && row.terminal == old->terminal &&
            row.definition == old->definition && row.path == old->path;
    });
    if(member == current->endpoints.end()) return {};
    const auto pin = ids.back();
    ids.pop_back();
    if(ids == current_occurrence_->path) return {};
    const auto target = simnodus::inspect_component_occurrence(*document_.graph(), ids);
    if(!target || target->component != member->definition ||
        std::none_of(target->terminals.begin(), target->terminals.end(), [&pin](const auto& row) { return row.pin == pin; })) return {};
    path.removeLast();
    return occurrence_view_choice_->findData(path) > 0 ? path : QStringList{};
}
void DocumentWindow::updatePeerAction()
{
    inspect_peer_->setEnabled(!peerOccurrencePath().isEmpty());
}
bool DocumentWindow::inspectSelectedPeer()
{
    // Copy identity before changing the selector: its callback destroys the member rows.
    const auto path = peerOccurrencePath();
    if(path.isEmpty()) {
        inspect_peer_->setEnabled(false);
        status_->setText("Peer inspection unavailable. Select a current endpoint of a different component; ports are not traversed.");
        return false;
    }
    occurrence_view_choice_->setCurrentIndex(occurrence_view_choice_->findData(path));
    status_->setText("Inspecting component " + path.join("/") + ". Pin selection and artwork capture remain explicit.");
    return true;
}
void DocumentWindow::updateOccurrenceNote(const QString& message)
{
    occurrence_view_note_->setToolTip({});
    QString note = "Select one existing component occurrence. Resource capture is explicit; placement and wiring are pending.";
    if(current_occurrence_) {
        const auto& current = *current_occurrence_;
        QStringList path;
        for(const auto& id : current.path) path << text(id);
        note = "Occurrence: " + path.join("/") + "\nSource: " + text(current.source_circuit) + "/" + text(current.source_instance) +
            "; component: " + text(current.component) + "\nCurrent instance name: " + text(current.name);
        if(const auto parameter = current.parameters.find("resistance"); parameter != current.parameters.end() && parameter->second.unit == "ohm")
            note += "\nApplied resistance: " + text(parameter->second.value) + " ohm (" + text(parameter->second.origin) + ")";
        note += "\nRead only; fitted artwork, no saved position or runtime measurement.";
        if(occurrence_capture_) {
            const auto& capture = *occurrence_capture_->symbol;
            note += "\nCaptured 227-byte owned fixture artwork.";
            if(capture.pins) {
                QStringList pins;
                for(const auto& pin : *capture.pins) pins << text(pin.logical_pin) + "/" + text(pin.symbol_pin);
                note += "\nPrivate fixture convention a=left, b=right: " + pins.join("; ");
            } else note += "\nPin-anchor correspondence unavailable.";
            occurrence_view_note_->setToolTip("Explicit occurrence root: " + text(capture.requested_root) +
                "\nPath: " + text(capture.selection.request.path) + "\nSHA-256: " + text(capture.selection.request.sha256));
        } else note += capture_occurrence_->isEnabled() ? "\nNo captured artwork. Choose an explicit resource root." :
            "\nArtwork unavailable for this component/descriptor.";
    }
    if(!message.isEmpty()) note += "\n" + message;
    occurrence_view_note_->setText(note);
}
bool DocumentWindow::captureOccurrence(const QString& resource_root)
{
    if(!document_.graph() || !current_occurrence_) return false;
    const auto result = simnodus::capture_fixture_occurrence(*document_.graph(), current_occurrence_->path, utf8(resource_root));
    if(const auto* error = std::get_if<simnodus::SymbolPreviewError>(&result)) {
        status_->setText(QString("Occurrence capture refused: %1 (system %2). Current document retained.").arg(error->code).arg(error->system_code));
        updateOccurrenceNote(occurrence_capture_ ? "Capture refused; prior occurrence capture retained." : "Capture unavailable.");
        return false;
    }
    occurrence_capture_ = std::get<0>(result);
    occurrence_artwork_->setCapture(occurrence_capture_->symbol);
    updateOccurrenceNote();
    status_->setText("Captured one existing occurrence's known artwork. No document edit or simulation.");
    return true;
}
void DocumentWindow::selectInstance()
{
    const auto* item = structure_->selectedItems().isEmpty() ? nullptr : structure_->currentItem();
    const auto circuit_id = item ? item->data(0, Qt::UserRole).toString() : QString{};
    const auto instance_id = item ? item->data(0, Qt::UserRole + 1).toString() : QString{};
    if(circuit_id == selected_circuit_ && instance_id == selected_instance_) return;
    if((instanceDraftPending() || resistanceDraftPending() || capacitanceDraftPending()) && QMessageBox::question(this, "Discard pending instance fields?",
        "Discard unapplied instance name/R/C text before changing selection?",
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
    return (document_.graph() && name_->text() != text(document_.name())) || instanceDraftPending() || resistanceDraftPending() || capacitanceDraftPending();
}
std::optional<simnodus::LiteralValueView> DocumentWindow::selectedResistance() const
{
    if(!document_.graph()) return {};
    return simnodus::literal_resistance(*document_.graph(), utf8(selected_circuit_), utf8(selected_instance_));
}
bool DocumentWindow::resistanceDraftPending() const
{
    const auto value = selectedResistance();
    return value && resistance_->text() != text(value->value);
}
std::optional<simnodus::LiteralValueView> DocumentWindow::selectedCapacitance() const
{
    if(!document_.graph()) return {};
    return simnodus::literal_capacitance(*document_.graph(), utf8(selected_circuit_), utf8(selected_instance_));
}
bool DocumentWindow::capacitanceDraftPending() const
{
    const auto value = selectedCapacitance();
    return value && capacitance_->text() != text(value->value);
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
        status_->setText("Apply or restore pending name/R/C text before Undo/Redo.");
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
void DocumentWindow::updateInstanceProperties(bool keep_name_draft, bool keep_resistance_draft, bool keep_capacitance_draft)
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
    const auto capacitance = selectedCapacitance();
    capacitance_->setEnabled(bool(capacitance)); apply_capacitance_->setEnabled(bool(capacitance));
    if(capacitance) {
        if(!keep_capacitance_draft) capacitance_->setText(text(capacitance->value));
        capacitance_limits_->setText("Capacitance value in " + text(capacitance->unit) + " (unit fixed).\nDeclared target limits: " +
            text(capacitance->minimum) + " to " + text(capacitance->maximum) + " " + text(capacitance->base_unit) + ".\nSource declaration edit; runtime remains unavailable.");
    } else {
        capacitance_->clear(); capacitance_limits_->setText("No supported literal capacitance override. Forwarded/default bindings are not editable here.");
    }
    updateHistoryActions();
    updateParameterInspection();
    retainPreview();
}
void DocumentWindow::updateParameterInspection()
{
    const auto previous = occurrence_->currentData().toStringList();
    const QSignalBlocker blocked(occurrence_);
    occurrence_->clear();
    if(document_.graph()) {
        const auto rows = simnodus::inspect_instance_parameters(*document_.graph(), utf8(selected_circuit_), utf8(selected_instance_));
        for(const auto* row : rows) {
            QStringList path;
            for(const auto& id : row->path) path << text(id);
            occurrence_->addItem(path.join("/"), path); // Presentation owns IDs, never borrowed rows.
        }
    }
    occurrence_->setEnabled(occurrence_->count() > 0);
    const auto retained = occurrence_->findData(previous);
    if(retained >= 0) occurrence_->setCurrentIndex(retained);
    selectOccurrence();
    refreshOccurrenceChoices();
}
void DocumentWindow::selectOccurrence()
{
    effective_parameters_->setRowCount(0);
    occurrence_note_->setText("No resolved occurrence for this selection. Applied declaration values only; runtime unavailable.");
    if(!document_.graph() || occurrence_->currentIndex() < 0) return;
    const auto selected = occurrence_->currentData().toStringList();
    // Resolve against the current retained graph after every edit/history restore.
    const auto rows = simnodus::inspect_instance_parameters(*document_.graph(), utf8(selected_circuit_), utf8(selected_instance_));
    for(const auto* occurrence : rows) {
        QStringList path;
        for(const auto& id : occurrence->path) path << text(id);
        if(path != selected) continue;
        occurrence_note_->setText("Occurrence: " + path.join("/") + "\nDefinition: " + text(occurrence->definition) +
            "\nApplied resolved declaration values; not measurements. Origin is the immediate binding.");
        for(const auto& [id, parameter] : occurrence->parameters) {
            if(!((id == "resistance" && parameter.unit == "ohm") || (id == "capacitance" && parameter.unit == "F"))) continue;
            const auto row = effective_parameters_->rowCount();
            effective_parameters_->insertRow(row);
            const QStringList cells{text(id), text(parameter.value) + " " + text(parameter.unit), text(parameter.origin)};
            for(int column = 0; column < cells.size(); ++column) {
                auto* item = new QTableWidgetItem(cells[column]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                item->setToolTip(cells[column]);
                effective_parameters_->setItem(row, column, item);
            }
        }
        if(effective_parameters_->rowCount() == 0) occurrence_note_->setText(occurrence_note_->text() + "\nNo closed R/C parameters declared.");
        return;
    }
}
bool DocumentWindow::applyInstanceName(const QString& name)
{
    if(!selectedInstance()) { status_->setText("Select a declared instance before editing its name."); return false; }
    const auto result = document_.rename_instance(utf8(selected_circuit_), utf8(selected_instance_), utf8(name));
    if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
        reportError("Instance name", error->code, error->offset, 0); return false;
    }
    updateInstanceProperties(false, true, true);
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
    updateInstanceProperties(true, false, true);
    setWindowTitle("SimNodus Circuit Editor - " + text(document_.name()) + (document_.dirty() ? " *" : ""));
    updateHistoryActions();
    status_->setText("Resistance value applied to the declaration. Save Copy remains explicit; simulation is unavailable.");
    return true;
}

bool DocumentWindow::applyCapacitance(const QString& value)
{
    if(!selectedCapacitance()) { status_->setText("Select an existing literal capacitance override before editing its value."); return false; }
    const auto result = document_.set_capacitance(utf8(selected_circuit_), utf8(selected_instance_), utf8(value));
    if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
        reportError("Capacitance value", error->code, error->offset, 0); return false;
    }
    updateInstanceProperties(true, true, false);
    setWindowTitle("SimNodus Circuit Editor - " + text(document_.name()) + (document_.dirty() ? " *" : ""));
    status_->setText("Capacitance value applied to the source declaration. Unit stays fixed; use Save Copy explicitly.");
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

void DocumentWindow::runCapacitanceAcceptance(const QString& root, const QString& report)
{
    // Focused fourth-field controls; reuse historical layout/worker evidence.
    // Scripted dialog responses do not establish human recovery or usability.
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
        checks["no_selection_refused"] = !capacitance_->isEnabled() && !applyCapacitance("470");
        structure_->setCurrentItem(instanceItem("main", "right"));
        const auto original = document_.graph();
        checks["fixed_unit_and_declared_target_bounds"] = capacitance_->text() == "220" && capacitance_->isEnabled() &&
            capacitance_limits_->text().contains("nF (unit fixed)") && capacitance_limits_->text().contains("0.000000001 to 0.001 F");
        capacitance_->setText("0.1"); apply_capacitance_->click();
        checks["outside_range_retains_graph_and_draft"] = document_.graph() == original && !document_.dirty() && capacitance_->text() == "0.1";
        checks["capacitance_draft_blocks_copy_and_history"] = !saveCopy("blocked.json") && !undoEdit() && !redoEdit() &&
            !undo_->isEnabled() && !redo_->isEnabled() && document_.graph() == original;
        answer(QMessageBox::Cancel);
        checks["open_cancel_retains_capacitance_draft"] = !confirmDiscard() && capacitanceDraftPending() && document_.graph() == original;
        answer(QMessageBox::Cancel); close();
        checks["exit_cancel_retains_capacitance_draft"] = isVisible() && capacitanceDraftPending() && document_.graph() == original;
        capacitance_->setText("NaN"); apply_capacitance_->click();
        checks["invalid_quantity_retains_graph_and_draft"] = document_.graph() == original && capacitance_->text() == "NaN";
        capacitance_->setText("220"); structure_->setCurrentItem(instanceItem("main", "left"));
        checks["missing_literal_refused"] = !capacitance_->isEnabled() && !applyCapacitance("470") && document_.graph() == original;
        structure_->setCurrentItem(instanceItem("rc", "c"));
        checks["forwarded_binding_refused"] = !capacitance_->isEnabled() && !apply_capacitance_->isEnabled() && !applyCapacitance("470") && document_.graph() == original;
        structure_->setCurrentItem(instanceItem("main", "right")); catalog_->setCurrentRow(0);
        const auto preview = preview_->text();
        name_->setText("Capacitance project"); instance_name_->setText("Edited right"); resistance_->setText("3.5"); capacitance_->setText("470");
        apply_->click();
        checks["project_apply_preserves_three_local_drafts"] = text(document_.name()) == "Capacitance project" &&
            instance_name_->text() == "Edited right" && resistance_->text() == "3.5" && capacitance_->text() == "470" &&
            instanceDraftPending() && resistanceDraftPending() && capacitanceDraftPending();
        apply_instance_->click();
        checks["instance_apply_preserves_both_numeric_drafts"] = text(selectedInstance()->identity.name) == "Edited right" &&
            resistance_->text() == "3.5" && capacitance_->text() == "470" && resistanceDraftPending() && capacitanceDraftPending();
        name_->setText("pending project"); instance_name_->setText("pending instance"); apply_resistance_->click();
        const auto resistance = document_.graph();
        checks["resistance_apply_preserves_capacitance_and_names"] = selectedResistance()->value == "3.5" && capacitance_->text() == "470" &&
            capacitanceDraftPending() && name_->text() == "pending project" && instance_name_->text() == "pending instance";
        resistance_->setText("4.7"); apply_capacitance_->click();
        const auto both = document_.graph();
        checks["capacitance_apply_preserves_resistance_and_names"] = both != resistance && selectedCapacitance()->value == "470" &&
            resistance_->text() == "4.7" && resistanceDraftPending() && name_->text() == "pending project" && instance_name_->text() == "pending instance";
        checks["remaining_drafts_block_copy_and_history"] = !saveCopy("blocked.json") && !undoEdit() && !redoEdit() &&
            !undo_->isEnabled() && !redo_->isEnabled() && document_.graph() == both;
        capacitance_->setText("330"); answer(QMessageBox::Cancel); structure_->setCurrentItem(instanceItem("main", "left"));
        checks["selection_cancel_retains_all_four_drafts"] = selected_instance_ == "right" && structure_->currentItem() == instanceItem("main", "right") &&
            name_->text() == "pending project" && instance_name_->text() == "pending instance" && resistance_->text() == "4.7" && capacitance_->text() == "330" && document_.graph() == both;
        answer(QMessageBox::Discard); structure_->setCurrentItem(instanceItem("main", "left"));
        checks["selection_discard_clears_three_local_drafts_only"] = selected_instance_ == "left" && !instanceDraftPending() && !resistanceDraftPending() &&
            !capacitanceDraftPending() && !capacitance_->isEnabled() && resistance_->text() == "1" && name_->text() == "pending project" && document_.graph() == both;
        name_->setText(text(document_.name())); structure_->setCurrentItem(instanceItem("main", "right"));
        checks["selected_fields_and_preview_retained"] = resistance_->text() == "3.5" && capacitance_->text() == "470" &&
            instance_name_->text() == "Edited right" && preview_->text() == preview && undo_->isEnabled();
        undo_->trigger();
        checks["capacitance_undo_retains_resistance_and_names"] = document_.graph() == resistance && capacitance_->text() == "220" &&
            resistance_->text() == "3.5" && instance_name_->text() == "Edited right" && name_->text() == "Capacitance project" && document_.dirty() && redo_->isEnabled();
        checks["one_step_only"] = !undo_->isEnabled() && !undoEdit() && document_.graph() == resistance && redo_->isEnabled();
        checks["undone_copy_retains_redo"] = saveCopy("capacitance-undone.json") && document_.graph() == resistance && document_.can_redo();
        capacitance_->setText("1000001"); apply_capacitance_->click();
        checks["invalid_edit_preserves_redo"] = document_.graph() == resistance && document_.can_redo() && capacitanceDraftPending();
        capacitance_->setText("220"); apply_capacitance_->click();
        checks["same_text_noop_preserves_redo"] = document_.graph() == resistance && redo_->isEnabled() && !editsPending();
        redo_->trigger();
        checks["redo_restores_all_applied_fields"] = document_.graph() == both && capacitance_->text() == "470" &&
            resistance_->text() == "3.5" && instance_name_->text() == "Edited right" && windowTitle().endsWith(" *");
        checks["explicit_copy_retains_association_and_history"] = saveCopy("capacitance-copy.json") && document_.graph() == both &&
            document_.leaf() == "original.json" && document_.dirty() && document_.can_undo();
        checks["source_and_occupied_copy_refused"] = !saveCopy("original.json") && !saveCopy("capacitance-copy.json") && document_.graph() == both;
        checks["failed_open_retains_all_selected_fields"] = !openDocument(root, "invalid.json") && document_.graph() == both &&
            selected_instance_ == "right" && capacitance_->text() == "470" && resistance_->text() == "3.5" && instance_name_->text() == "Edited right";
        checks["explicit_reopen_resets_history_and_baseline"] = openDocument(root, "capacitance-copy.json") && !document_.dirty() &&
            !document_.can_undo() && !document_.can_redo() && selected_instance_.isEmpty();
        structure_->setCurrentItem(instanceItem("main", "right"));
        checks["reopened_fields_match_persisted_copy"] = capacitance_->text() == "470" && resistance_->text() == "3.5" &&
            instance_name_->text() == "Edited right" && name_->text() == "Capacitance project" && !editsPending();
        checks["initial_graph_remains_immutable"] = simnodus::literal_capacitance(*original, "main", "right")->value == "220" &&
            simnodus::literal_resistance(*original, "main", "right")->value == "2.2";
        const auto final = document_.graph(); showAnalyzer(); analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_preserves_document"] = analyzer_.isVisible() && document_.graph() == final;
    }
    bool passed = checks.size() == 31;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"capacitance_limits_text", capacitance_limits_->text()},
        {"scope", "scripted closed capacitance value and four-draft mixed history; no engine, resource or human recovery acceptance"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runInspectionAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    const auto displays = [this](const QString& id, const QString& value, const QString& origin) {
        for(int row = 0; row < effective_parameters_->rowCount(); ++row)
            if(effective_parameters_->item(row, 0)->text() == id)
                return effective_parameters_->item(row, 1)->text() == value && effective_parameters_->item(row, 2)->text() == origin;
        return false;
    };
    showAnalyzer();
    checks["native_independent_windows"] = QApplication::platformName() == "windows" && isWindow() && analyzer_.isWindow() && analyzer_.isVisible();
    checks["initial_empty_inspection"] = occurrence_->count() == 0 && !occurrence_->isEnabled() && effective_parameters_->rowCount() == 0;
    checks["inert_open"] = openDocument(root, "original.json");
    if(document_.graph()) {
        const auto original = document_.graph();
        const auto bytes = std::string(document_.bytes());
        checks["no_runtime_resource_authority"] = !original->declaration->runtime_profile_verified &&
            !original->declaration->firmware_verified && !original->declaration->sources.resource_interfaces_verified;
        catalog_->setCurrentRow(0);
        const auto preview = preview_->text();
        structure_->setCurrentItem(instanceItem("main", "left"));
        checks["literal_default_base_values"] = occurrence_->currentText() == "main/left" && effective_parameters_->rowCount() == 2 &&
            displays("resistance", "1000 ohm", "literal") && displays("capacitance", "0.000001 F", "default");
        checks["default_inspection_does_not_enable_edit"] = !capacitance_->isEnabled() && !apply_capacitance_->isEnabled();
        const auto* group = instanceItem("main", "left")->parent();
        structure_->setCurrentItem(const_cast<QTreeWidgetItem*>(group));
        checks["circuit_selection_has_no_occurrence"] = occurrence_->count() == 0 && effective_parameters_->rowCount() == 0;
        structure_->setCurrentItem(group->child(2));
        checks["net_selection_has_no_occurrence"] = occurrence_->count() == 0 && !occurrence_->isEnabled() && effective_parameters_->rowCount() == 0;
        structure_->setCurrentItem(instanceItem("rc", "r"));
        checks["reused_full_paths_sorted"] = occurrence_->count() == 2 && occurrence_->itemText(0) == "main/left/r" && occurrence_->itemText(1) == "main/right/r";
        checks["left_forwarded_binding"] = effective_parameters_->rowCount() == 1 && displays("resistance", "1000 ohm", "containing-circuit:resistance");
        occurrence_->setCurrentIndex(1);
        checks["right_forwarded_binding"] = displays("resistance", "2200 ohm", "containing-circuit:resistance") &&
            occurrence_note_->text().contains("Definition: resistor") && occurrence_note_->text().contains("not measurements");
        checks["read_only_plain_text"] = effective_parameters_->editTriggers() == QAbstractItemView::NoEditTriggers &&
            !(effective_parameters_->item(0, 1)->flags() & Qt::ItemIsEditable) && occurrence_note_->textFormat() == Qt::PlainText;
        name_->setText("Pending project"); instance_name_->setText("Pending resistor");
        occurrence_->setCurrentIndex(0); occurrence_->setCurrentIndex(1);
        checks["actual_occurrence_switch_retains_two_active_drafts"] = name_->text() == "Pending project" && instance_name_->text() == "Pending resistor" &&
            !resistance_->isEnabled() && !capacitance_->isEnabled() && displays("resistance", "2200 ohm", "containing-circuit:resistance");
        checks["occurrence_switch_nonmutation"] = document_.graph() == original && document_.bytes() == bytes && !document_.dirty() &&
            !document_.can_undo() && !document_.can_redo() && document_.leaf() == "original.json" && preview_->text() == preview;
        name_->setText(text(document_.name())); instance_name_->setText(text(selectedInstance()->identity.name));
        checks["name_apply_retains_reused_path"] = applyInstanceName("Inspection resistor") && occurrence_->currentText() == "main/right/r" &&
            displays("resistance", "2200 ohm", "containing-circuit:resistance");
        checks["name_restore_retains_reused_path"] = undoEdit() && document_.graph() == original && occurrence_->currentText() == "main/right/r" &&
            redoEdit() && occurrence_->currentText() == "main/right/r" && undoEdit();
        structure_->setCurrentItem(instanceItem("main", "right"));
        name_->setText("Inspection project"); instance_name_->setText("Inspection right");
        resistance_->setText("3.5"); capacitance_->setText("470");
        updateParameterInspection();
        checks["single_occurrence_refresh_retains_four_active_drafts"] = name_->text() == "Inspection project" && instance_name_->text() == "Inspection right" &&
            resistance_->text() == "3.5" && capacitance_->text() == "470" && name_->isEnabled() && instance_name_->isEnabled() &&
            resistance_->isEnabled() && capacitance_->isEnabled() && occurrence_->count() == 1;
        checks["drafts_do_not_change_applied_values"] = displays("resistance", "2200 ohm", "literal") && displays("capacitance", "0.000000220 F", "literal") && document_.graph() == original;
        checks["pending_drafts_block_copy_and_history"] = !saveCopy("unapplied.json") && !undoEdit() && !redoEdit();
        apply_resistance_->click();
        checks["resistance_apply_refresh_preserves_other_drafts"] = displays("resistance", "3500 ohm", "literal") && displays("capacitance", "0.000000220 F", "literal") &&
            capacitance_->text() == "470" && name_->text() == "Inspection project" && instance_name_->text() == "Inspection right";
        apply_capacitance_->click();
        checks["capacitance_apply_refresh_preserves_name_drafts"] = displays("capacitance", "0.000000470 F", "literal") && displays("resistance", "3500 ohm", "literal") &&
            name_->text() == "Inspection project" && instance_name_->text() == "Inspection right";
        apply_->click();
        checks["project_name_refresh_preserves_instance_draft"] = document_.name() == "Inspection project" && instance_name_->text() == "Inspection right" &&
            occurrence_->currentText() == "main/right" && displays("capacitance", "0.000000470 F", "literal");
        apply_instance_->click();
        const auto final = document_.graph();
        checks["instance_name_refresh_preserves_applied_values"] = !editsPending() && occurrence_->currentText() == "main/right" &&
            displays("resistance", "3500 ohm", "literal") && displays("capacitance", "0.000000470 F", "literal");
        checks["explicit_copy_retains_source_history"] = saveCopy("inspected.json") && document_.graph() == final && document_.dirty() &&
            document_.can_undo() && document_.leaf() == "original.json" && occurrence_->currentText() == "main/right";
        structure_->setCurrentItem(instanceItem("rc", "r")); occurrence_->setCurrentIndex(1);
        checks["upstream_edit_refreshes_reused_occurrence"] = displays("resistance", "3500 ohm", "containing-circuit:resistance");
        checks["undo_requeries_current_graph_retains_path"] = undoEdit() && occurrence_->currentText() == "main/right/r" &&
            displays("resistance", "3500 ohm", "containing-circuit:resistance") && saveCopy("inspection-undone.json");
        checks["redo_requeries_current_graph_retains_path"] = redoEdit() && document_.graph() == final && occurrence_->currentText() == "main/right/r" &&
            displays("resistance", "3500 ohm", "containing-circuit:resistance");
        checks["failed_edit_open_copy_keep_occurrence"] = !applyInstanceName("") && !openDocument(root, "invalid.json") && !saveCopy("inspected.json") &&
            document_.graph() == final && occurrence_->currentText() == "main/right/r" && document_.can_undo();
        checks["explicit_reopen_clears_selection_and_history"] = openDocument(root, "inspected.json") && occurrence_->count() == 0 &&
            !document_.dirty() && !document_.can_undo() && !document_.can_redo();
        const auto reopened_preview = preview_->text();
        structure_->setCurrentItem(instanceItem("main", "right"));
        checks["reopened_resolved_values"] = displays("resistance", "3500 ohm", "literal") && displays("capacitance", "0.000000470 F", "literal");
        resize(1100, 560); resizeDocks({properties_}, {350}, Qt::Horizontal);
        QApplication::processEvents();
        properties_scroll_->ensureWidgetVisible(apply_capacitance_);
        QApplication::processEvents();
        const QRect button_rect(apply_capacitance_->mapTo(properties_scroll_->viewport(), QPoint{}), apply_capacitance_->size());
        checks["short_panel_existing_control_reachable"] = properties_scroll_->verticalScrollBar()->maximum() > 0 &&
            properties_scroll_->viewport()->rect().contains(button_rect);
        properties_scroll_->verticalScrollBar()->setValue(0);
        QApplication::processEvents();
        checks["inspection_reachable_after_scroll"] = properties_scroll_->viewport()->rect().contains(
            QRect(occurrence_->mapTo(properties_scroll_->viewport(), QPoint{}), occurrence_->size()));
        checks["preview_and_original_snapshot_retained"] = preview_->text() == reopened_preview &&
            original->declaration->sources.lock.syntax->bytes == bytes;
    }
    bool passed = checks.size() == 33;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"occurrence_text", occurrence_note_->text()},
        {"scope", "scripted applied declaration inspection; two active drafts across reused paths and four across single-path refresh; no measurements or human usability acceptance"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runPreviewAcceptance(const QString& root, const QString& resource_root, const QString& report)
{
    QJsonObject checks;
    showAnalyzer();
    checks["native_independent_windows"] = QApplication::platformName() == "windows" && isWindow() && analyzer_.isWindow() && analyzer_.isVisible();
    checks["initial_empty_artwork"] = !artwork_->capture() && !preview_artwork_->isEnabled();
    checks["inert_open"] = openDocument(root, "original.json");
    if(document_.graph()) {
        const auto original = document_.graph();
        const auto bytes = std::string(document_.bytes());
        int resistor = -1, capacitor = -1;
        const auto& components = original->connectivity.components;
        for(std::size_t i = 0; i < components.size(); ++i) {
            if(components[i].identity.id == "resistor") resistor = static_cast<int>(i);
            if(components[i].identity.id == "capacitor") capacitor = static_cast<int>(i);
        }
        catalog_->setCurrentRow(resistor);
        checks["catalog_selection_no_capture"] = !artwork_->capture() && preview_artwork_->isEnabled() && document_.graph() == original;
        structure_->setCurrentItem(instanceItem("main", "right"));
        const auto inspector = inspector_->text(), path = occurrence_->currentText();
        name_->setText("Preview project"); instance_name_->setText("Preview right");
        resistance_->setText("3.5"); capacitance_->setText("470");
        checks["explicit_selected_symbol_capture"] = previewArtwork(resource_root) && artwork_->capture() && artwork_->capture()->resources->size() == 1;
        const auto captured = artwork_->capture();
        checks["four_drafts_retained"] = name_->text() == "Preview project" && instance_name_->text() == "Preview right" &&
            resistance_->text() == "3.5" && capacitance_->text() == "470";
        checks["capture_nonmutation_and_independent_properties"] = document_.graph() == original && document_.bytes() == bytes && !document_.dirty() &&
            !document_.can_undo() && !document_.can_redo() && inspector_->text() == inspector && occurrence_->currentText() == path && document_.leaf() == "original.json";
        checks["capture_origin_and_unverified_anchors"] = captured && captured->requested_root == utf8(resource_root) &&
            captured->selection.component == "resistor" && artwork_note_->text().contains("fixture artwork") &&
            artwork_note_->text().contains("Fixture anchor convention") && artwork_note_->toolTip().contains(text(captured->selection.request.sha256));
        checks["metadata_plain_text_and_no_global_authority"] = artwork_note_->textFormat() == Qt::PlainText && preview_->textFormat() == Qt::PlainText &&
            !original->declaration->sources.lock.resources_verified && !original->declaration->sources.resource_interfaces_verified && !original->declaration->runtime_profile_verified;
        checks["failed_recapture_retains_prior_origin"] = !previewArtwork(resource_root + "/missing") && artwork_->capture() == captured &&
            captured && captured->requested_root == utf8(resource_root) && artwork_note_->text().contains("prior captured artwork retained") && document_.graph() == original;
        if(captured) {
            const auto source_path = resource_root + "/" + text(captured->selection.request.path);
            QFile source(source_path);
            checks["replace_fixture_source"] = source.open(QIODevice::WriteOnly | QIODevice::Truncate) && source.write("replaced source") == 15;
            source.close();
            const auto& art_bytes = captured->resources->front().data;
            checks["captured_bytes_survive_replacement"] = simnodus::symbols::recognize_fixture_artwork(
                std::string_view(reinterpret_cast<const char*>(art_bytes.data()), art_bytes.size())).has_value();
            checks["changed_file_recapture_refused"] = !previewArtwork(resource_root) && artwork_->capture() == captured && document_.graph() == original;
        }
        const auto fits = [](PreviewCanvas& canvas) {
            const auto view = canvas.fittedView();
            return !view.isEmpty() && qAbs(view.width() / view.height() - 2.5) < 1e-12 &&
                view.left() >= 12 && view.top() >= 12 && view.right() <= canvas.width() - 12 && view.bottom() <= canvas.height() - 12;
        };
        PreviewCanvas wide; wide.setCapture(captured); wide.resize(420, 140);
        PreviewCanvas narrow; narrow.setCapture(captured); narrow.resize(120, 220);
        const auto painted = [](PreviewCanvas& canvas, const QImage& image) {
            if(!canvas.capture() || image.pixelColor(2, 2).value() < 250) return false;
            const auto view = canvas.fittedView();
            const auto& geometry = canvas.capture()->artwork;
            for(const auto& line : geometry.lines) {
                const auto point = (QPointF(view.left() + (line.x1 + line.x2) / 2 * view.width() / geometry.width,
                    view.top() + (line.y1 + line.y2) / 2 * view.height() / geometry.height) * image.devicePixelRatio()).toPoint();
                bool dark = false;
                for(int x = point.x() - 2; x <= point.x() + 2; ++x)
                    for(int y = point.y() - 2; y <= point.y() + 2; ++y)
                        dark = dark || image.pixelColor(x, y).value() < 80;
                if(!dark) return false;
            }
            return true;
        };
        const auto wide_image = wide.grab().toImage(), narrow_image = narrow.grab().toImage();
        checks["wide_aspect_fit_and_capture"] = fits(wide) && painted(wide, wide_image) && wide_image.save(report + ".wide.png");
        checks["narrow_aspect_fit_and_capture"] = fits(narrow) && painted(narrow, narrow_image) && narrow_image.save(report + ".narrow.png");
        checks["fit_does_not_recapture_or_edit"] = wide.capture() == captured && narrow.capture() == captured && document_.graph() == original && document_.bytes() == bytes;
        if(captured) {
            QFile source(resource_root + "/" + text(captured->selection.request.path));
            const auto& art_bytes = captured->resources->front().data;
            checks["restore_owned_test_source"] = source.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                source.write(reinterpret_cast<const char*>(art_bytes.data()), static_cast<qint64>(art_bytes.size())) == static_cast<qint64>(art_bytes.size());
        }
        apply_resistance_->click(); apply_capacitance_->click(); apply_->click(); apply_instance_->click();
        const auto edited = document_.graph();
        checks["applies_retain_capture_and_selections"] = artwork_->capture() == captured && document_.dirty() && !editsPending() &&
            selected_instance_ == "right" && occurrence_->currentText() == "main/right" && selectedComponent() == "resistor";
        checks["copy_preserves_capture_and_history"] = saveCopy("preview-copy.json") && artwork_->capture() == captured && document_.graph() == edited &&
            document_.dirty() && document_.can_undo() && document_.leaf() == "original.json";
        checks["undo_preserves_capture_and_path"] = undoEdit() && artwork_->capture() == captured && occurrence_->currentText() == "main/right" &&
            saveCopy("preview-undone.json");
        checks["redo_preserves_capture"] = redoEdit() && document_.graph() == edited && artwork_->capture() == captured;
        structure_->setCurrentItem(instanceItem("rc", "r")); occurrence_->setCurrentIndex(1);
        checks["instance_occurrence_selection_preserves_preview"] = artwork_->capture() == captured && selectedComponent() == "resistor" && occurrence_->currentText() == "main/right/r";
        checks["failed_open_preserves_capture"] = !openDocument(root, "invalid.json") && artwork_->capture() == captured && document_.graph() == edited;
        catalog_->setCurrentRow(capacitor);
        checks["unsupported_catalog_clears_artifact_and_origin"] = !artwork_->capture() && !preview_artwork_->isEnabled() && artwork_note_->toolTip().isEmpty() &&
            selected_circuit_ == "rc" && selected_instance_ == "r" && occurrence_->currentText() == "main/right/r";
        catalog_->setCurrentRow(resistor);
        checks["catalog_return_requires_explicit_capture"] = !artwork_->capture() && preview_artwork_->isEnabled();
        checks["explicit_recapture_after_selection"] = previewArtwork(resource_root) && artwork_->capture() != captured && document_.graph() == edited;
        checks["successful_open_clears_capture_and_history"] = openDocument(root, "preview-copy.json") && !artwork_->capture() &&
            !document_.dirty() && !document_.can_undo() && !document_.can_redo() && selected_instance_.isEmpty();
        catalog_->setCurrentRow(resistor); structure_->setCurrentItem(instanceItem("main", "right"));
        checks["explicit_reopened_preview"] = previewArtwork(resource_root) && document_.graph() && !document_.dirty() &&
            instance_name_->text() == "Preview right" && resistance_->text() == "3.5" && capacitance_->text() == "470";
        checks["retained_initial_document_and_capture"] = original->declaration->sources.lock.syntax->bytes == bytes && captured &&
            captured->resources->front().data.size() == 227;
        const auto final = document_.graph(); analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_preserves_document_and_artwork"] = analyzer_.isVisible() && document_.graph() == final && artwork_->capture();
        QApplication::processEvents();
        bool labels_fit = true;
        for(auto* label : {inspector_, occurrence_note_, resistance_limits_, capacitance_limits_})
            labels_fit = labels_fit && label->height() >= label->heightForWidth(label->width());
        properties_scroll_->ensureWidgetVisible(apply_capacitance_);
        QApplication::processEvents();
        properties_scroll_->verticalScrollBar()->setValue(properties_scroll_->verticalScrollBar()->maximum());
        QApplication::processEvents();
        checks["properties_text_fit_and_last_control_reachable"] = labels_fit && properties_scroll_->verticalScrollBar()->maximum() > 0 &&
            properties_scroll_->viewport()->rect().contains(QRect(apply_capacitance_->mapTo(properties_scroll_->viewport(), QPoint{}), apply_capacitance_->size())) &&
            properties_->grab().save(report + ".properties-bottom.png");
        properties_scroll_->verticalScrollBar()->setValue(0);
    }
    bool passed = checks.size() == 31;
    for(const auto value : checks) passed = passed && value.toBool();
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"artwork_note", artwork_note_->text()}, {"artwork_origin", artwork_note_->toolTip()},
        {"scope", "one explicit known fixture artwork; no SVG parser, pin-anchor/electrical truth, automatic resource access or human dialog/DPI acceptance"}};
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runPinPreviewAcceptance(const QString& root, const QString& resource_root, const QString& report)
{
    QJsonObject checks;
    checks["initial_inert_open"] = openDocument(root, "original.json") && !artwork_->capture() && !document_.dirty();
    if(document_.graph()) {
        int resistor = -1;
        const auto& components = document_.graph()->connectivity.components;
        for(std::size_t i = 0; i < components.size(); ++i)
            if(components[i].identity.id == "resistor") resistor = static_cast<int>(i);
        catalog_->setCurrentRow(resistor);
        structure_->setCurrentItem(instanceItem("main", "right"));
        const auto original = document_.graph();
        const auto bytes = std::string(document_.bytes());
        const auto inspector = inspector_->text(), path = occurrence_->currentText();
        name_->setText("pending project"); instance_name_->setText("pending instance"); resistance_->setText("3.5"); capacitance_->setText("470");
        checks["explicit_selected_only_pin_capture"] = previewArtwork(resource_root) && artwork_->capture() &&
            artwork_->capture()->resources->size() == 1 && artwork_->capture()->pins;
        // Caption changes can trigger a dock width change followed by wrapped-height
        // relayout. Settle the bounded test's nested layout events before inspection.
        for(int i = 0; i < 4; ++i) QApplication::processEvents();
        const auto captured = artwork_->capture();
        checks["original_declared_mapping_and_caption"] = captured && captured->pins &&
            (*captured->pins)[0].logical_pin == "p" && (*captured->pins)[0].symbol_pin == "a" &&
            (*captured->pins)[1].logical_pin == "n" && (*captured->pins)[1].symbol_pin == "b" &&
            artwork_note_->textFormat() == Qt::PlainText && artwork_note_->text().contains("Fixture anchor convention") &&
            artwork_note_->text().contains("Declared mapping only") && artwork_note_->height() >= artwork_note_->heightForWidth(artwork_note_->width()) &&
            preview_->height() >= preview_->heightForWidth(preview_->width());
        checks["four_drafts_and_independent_properties_retained"] = name_->text() == "pending project" && instance_name_->text() == "pending instance" &&
            resistance_->text() == "3.5" && capacitance_->text() == "470" && inspector_->text() == inspector && occurrence_->currentText() == path;
        checks["pins_create_no_document_or_runtime_authority"] = document_.graph() == original && document_.bytes() == bytes && !document_.dirty() &&
            !document_.can_undo() && !original->declaration->sources.lock.resources_verified && !original->declaration->sources.resource_interfaces_verified &&
            !original->declaration->runtime_profile_verified;
        checks["failed_recapture_preserves_owned_pins"] = !previewArtwork(resource_root + "/missing") && artwork_->capture() == captured &&
            document_.graph() == original && artwork_note_->text().contains("prior captured artwork retained");
        const auto markers = [](PreviewCanvas& canvas, const QString& output) {
            const auto image = canvas.grab().toImage();
            if(!canvas.capture() || !canvas.capture()->pins) return false;
            const auto view = canvas.fittedView();
            if(qAbs(view.width() / view.height() - 2.5) > 1e-12) return false;
            for(const auto& pin : *canvas.capture()->pins) {
                const auto p = (QPointF(view.left() + pin.x / 100 * view.width(), view.top() + pin.y / 40 * view.height()) * image.devicePixelRatio()).toPoint();
                const auto color = image.pixelColor(p);
                if(color.red() > 30 || color.blue() < 150 || color.green() < 50 || color.green() > 130) return false;
            }
            return image.save(output);
        };
        PreviewCanvas wide; wide.setCapture(captured); wide.resize(420, 180);
        PreviewCanvas narrow; narrow.setCapture(captured); narrow.resize(120, 220);
        checks["wide_markers_follow_same_fit"] = markers(wide, report + ".wide.png");
        checks["narrow_markers_follow_same_fit"] = markers(narrow, report + ".narrow.png");
        name_->setText(text(document_.name())); instance_name_->setText("right"); capacitance_->setText("220");
        checks["existing_resistance_edit_retains_pin_capture"] = applyResistance("3.5") && artwork_->capture() == captured && !editsPending();
        const auto edited = document_.graph();
        checks["existing_copy_retains_pins_and_dirty_association"] = saveCopy("pin-copy.json") && artwork_->capture() == captured && document_.dirty() && document_.leaf() == "original.json";
        checks["one_step_history_retains_pins"] = undoEdit() && artwork_->capture() == captured && redoEdit() && document_.graph() == edited && artwork_->capture() == captured;
        checks["failed_open_retains_pins_and_document"] = !openDocument(root, "invalid.json") && artwork_->capture() == captured && document_.graph() == edited;
        checks["copy_reopen_inert_and_exact"] = openDocument(root, "pin-copy.json") && !artwork_->capture() && !document_.dirty() && !document_.can_undo();
        catalog_->setCurrentRow(resistor);
        checks["explicit_copy_recapture_keeps_same_mapping"] = previewArtwork(resource_root) && artwork_->capture()->pins == captured->pins;
        checks["swapped_document_open_clears_capture"] = openDocument(root, "swapped.json") && !artwork_->capture();
        catalog_->setCurrentRow(resistor);
        const auto swapped_preview = previewArtwork(resource_root);
        for(int i = 0; i < 4; ++i) QApplication::processEvents();
        checks["swapped_mapping_annotations"] = swapped_preview && artwork_->capture()->pins &&
            (*artwork_->capture()->pins)[0].logical_pin == "n" && (*artwork_->capture()->pins)[1].logical_pin == "p" &&
            artwork_note_->text().contains("n -> a; p -> b") && artwork_note_->height() >= artwork_note_->heightForWidth(artwork_note_->width()) &&
            grab().save(report + ".swapped.png");
        openDocument(root, "alternate.json"); catalog_->setCurrentRow(resistor);
        checks["unsupported_anchors_keep_only_artwork"] = previewArtwork(resource_root) && artwork_->capture() && !artwork_->capture()->pins &&
            artwork_note_->text().contains("correspondence unavailable");
        const auto alternate = artwork_->capture();
        document_.open(utf8(root), "alternate-reordered.json"); retainPreview();
        checks["equivalent_unavailable_map_retains_capture"] = artwork_->capture() == alternate;
        document_.open(utf8(root), "alternate-swapped.json"); retainPreview();
        checks["changed_unavailable_map_clears_capture"] = !artwork_->capture() && artwork_note_->text().contains("Explicit new Preview required");
        openDocument(root, "pin-copy.json"); catalog_->setCurrentRow(resistor); structure_->setCurrentItem(instanceItem("main", "right")); previewArtwork(resource_root);
        const auto final = document_.graph(); showAnalyzer(); analyzer_.close(); showAnalyzer();
        checks["independent_analyzer_and_initial_source_preserved"] = analyzer_.isVisible() && document_.graph() == final && artwork_->capture()->pins &&
            original->declaration->sources.lock.syntax->bytes == bytes;
    }
    bool passed = checks.size() == 20;
    for(const auto value : checks) passed = passed && value.toBool();
    QApplication::processEvents();
    if(!grab().save(report + ".png")) { QApplication::exit(3); return; }
    QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"caption", artwork_note_->text()}, {"scope", "declared logical pins plus private exact fixture anchor convention; no SVG/electrical truth, placement or wiring"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runOccurrenceAcceptance(const QString& root, const QString& resource_root, const QString& report)
{
    QJsonObject checks;
    showAnalyzer();
    checks["native_independent_windows"] = QApplication::platformName() == "windows" && isWindow() && analyzer_.isWindow() && analyzer_.isVisible();
    checks["inert_open_and_empty_occurrence"] = openDocument(root, "original.json") && !current_occurrence_ && !occurrence_capture_ &&
        !artwork_->capture() && !document_.dirty() && occurrence_view_choice_->count() == 5 && occurrence_terminals_->rowCount() == 0;
    const auto choose = [this](const QStringList& path) {
        const auto index = occurrence_view_choice_->findData(path);
        if(index < 0) return false;
        occurrence_view_choice_->setCurrentIndex(index);
        return occurrence_view_choice_->currentData().toStringList() == path;
    };
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const QStringList right{"main", "right", "r"}, left{"main", "left", "r"}, capacitor{"main", "right", "c"};
    const auto terminals = [this](const QString& path, const QString& n, const QString& p) {
        if(occurrence_terminals_->rowCount() != 2) return false;
        for(int row = 0; row < 2; ++row) {
            const auto id = row == 0 ? "n" : "p";
            const auto net = row == 0 ? n : p;
            const auto net_path = net.isEmpty() ? QString{} : path + "/" + net;
            if(!occurrence_terminals_->item(row, 0) || occurrence_terminals_->item(row, 0)->text() != id ||
                occurrence_terminals_->item(row, 2)->text() != (net.isEmpty() ? "Unconnected locally" : net) ||
                occurrence_terminals_->item(row, 4)->text() != net_path) return false;
        }
        return true;
    };
    const auto pin = [this](const QString& id) {
        for(int row = 0; row < occurrence_terminals_->rowCount(); ++row) {
            if(occurrence_terminals_->item(row, 0)->data(Qt::UserRole).toString() != id) continue;
            occurrence_terminals_->setCurrentCell(row, 0);
            return selected_terminal_ == id;
        }
        return false;
    };
    const auto endpoint = [this](const QStringList& path) {
        for(int row = 0; row < local_endpoints_->rowCount(); ++row) {
            if(local_endpoints_->item(row, 0)->data(Qt::UserRole).toStringList() != path) continue;
            local_endpoints_->setCurrentCell(row, 3);
            return local_endpoints_->selectedItems().size() == 4;
        }
        return false;
    };
    const auto junction = [this](const QString& parent) {
        return selected_terminal_ == "n" && current_local_net_ && current_local_net_->net.id == "junction" && local_endpoints_->rowCount() == 3 &&
            local_endpoints_->item(0, 0)->text() == "Local circuit port" && local_endpoints_->item(0, 3)->text() == parent + "/output" &&
            local_endpoints_->item(1, 0)->text() == "Component pin" && local_endpoints_->item(1, 3)->text() == parent + "/c/p" &&
            local_endpoints_->item(2, 3)->text() == parent + "/r/n";
    };
    const auto occurrence_root = resource_root + "/occurrence", library_root = resource_root + "/library";
    if(document_.graph()) {
        const auto original = document_.graph();
        const auto bytes = std::string(document_.bytes());
        checks["explicit_existing_occurrence_selection"] = choose(right) && current_occurrence_ &&
            current_occurrence_->source_circuit == "rc" && current_occurrence_->source_instance == "r" &&
            current_occurrence_->parameters.at("resistance").value == "2200" && capture_occurrence_->isEnabled() && !occurrence_capture_;
        checks["declared_local_terminals_without_artwork"] = terminals("main/right", "junction", "drive") &&
            !occurrence_capture_ && !artwork_->capture() && document_.graph() == original && document_.bytes() == bytes;
        checks["no_implicit_pin_or_net_selection"] = selected_terminal_.isEmpty() && !current_local_net_ && local_endpoints_->rowCount() == 0;
        checks["explicit_pin_selects_all_local_members_inertly"] = pin("n") && junction("main/right") &&
            document_.graph() == original && document_.bytes() == bytes && !occurrence_capture_ && !document_.dirty();
        bool member_read_only = local_endpoints_->editTriggers() == QAbstractItemView::NoEditTriggers && local_endpoints_note_->textFormat() == Qt::PlainText;
        for(int row = 0; row < local_endpoints_->rowCount(); ++row) for(int column = 0; column < 4; ++column)
            member_read_only = member_read_only && !(local_endpoints_->item(row, column)->flags() & Qt::ItemIsEditable) && local_endpoints_->item(row, column)->toolTip().isEmpty();
        checks["member_cells_plain_read_only_without_tooltips"] = member_read_only;
        bool read_only = occurrence_terminals_->editTriggers() == QAbstractItemView::NoEditTriggers;
        for(int row = 0; row < occurrence_terminals_->rowCount(); ++row) for(int column = 0; column < 5; ++column)
            read_only = read_only && !(occurrence_terminals_->item(row, column)->flags() & Qt::ItemIsEditable) &&
                occurrence_terminals_->item(row, column)->toolTip().isEmpty();
        checks["terminal_table_read_only_ids_and_labels"] = read_only && occurrence_terminals_->item(0, 1)->text() == "2" &&
            occurrence_terminals_->item(1, 1)->text() == "1" && occurrence_terminals_->item(1, 3)->text() == "drive";
        int resistor = -1, cap = -1;
        for(std::size_t i = 0; i < original->connectivity.components.size(); ++i) {
            const auto& id = original->connectivity.components[i].identity.id;
            if(id == "resistor") resistor = static_cast<int>(i);
            if(id == "capacitor") cap = static_cast<int>(i);
        }
        catalog_->setCurrentRow(resistor);
        structure_->setCurrentItem(instanceItem("main", "right"));
        name_->setText("Pending project"); instance_name_->setText("Pending right");
        resistance_->setText("3.5"); capacitance_->setText("470");
        const auto drafts = [this] {
            return name_->text() == "Pending project" && instance_name_->text() == "Pending right" && resistance_->text() == "3.5" && capacitance_->text() == "470";
        };
        checks["independent_library_capture"] = previewArtwork(library_root) && artwork_->capture() && !occurrence_capture_;
        const auto library = artwork_->capture();
        checks["independent_occurrence_capture"] = captureOccurrence(occurrence_root) && occurrence_capture_ && library &&
            occurrence_capture_->symbol != library && occurrence_capture_->symbol->requested_root == utf8(occurrence_root) &&
            library->requested_root == utf8(library_root) && occurrence_capture_->symbol->resources->size() == 1;
        checks["capture_nonmutation_and_four_drafts"] = drafts() && document_.graph() == original && document_.bytes() == bytes &&
            !document_.dirty() && !document_.can_undo() && !original->declaration->sources.lock.resources_verified &&
            !original->declaration->sources.lock.containment_verified && !original->declaration->sources.resource_interfaces_verified && !original->declaration->runtime_profile_verified;
        checks["pin_and_properties_library_keep_four_drafts_and_members"] = drafts() && junction("main/right") &&
            pin("p") && current_local_net_ && current_local_net_->net.id == "drive" && local_endpoints_->rowCount() == 2 &&
            pin("n") && junction("main/right") && drafts();
        const auto before_peer = occurrence_capture_;
        checks["empty_peer_selection_refuses_inertly"] = !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            occurrence_capture_ == before_peer && drafts() && document_.graph() == original;
        checks["peer_row_selection_is_inert"] = endpoint({"main", "right", "c", "p"}) && inspect_peer_->isEnabled() &&
            occurrence_view_choice_->currentData().toStringList() == right && occurrence_capture_ == before_peer &&
            document_.graph() == original && document_.bytes() == bytes && !document_.dirty() && !document_.can_undo() && drafts();
        inspect_peer_->click();
        checks["explicit_peer_navigation_preserves_four_drafts_and_independent_views"] =
            occurrence_view_choice_->currentData().toStringList() == capacitor && current_occurrence_ && current_occurrence_->component == "capacitor" &&
            drafts() && selected_circuit_ == "main" && selected_instance_ == "right" && occurrence_->currentText() == "main/right" &&
            selectedComponent() == "resistor" && artwork_->capture() == library && document_.graph() == original && document_.bytes() == bytes;
        checks["peer_destination_requires_explicit_pin_and_artwork"] = !occurrence_capture_ && !occurrence_artwork_->capture() &&
            selected_terminal_.isEmpty() && !current_local_net_ && local_endpoints_->rowCount() == 0 && !inspect_peer_->isEnabled() &&
            !capture_occurrence_->isEnabled() && analyzer_.isVisible();
        checks["local_port_and_self_cannot_be_peer_destinations"] = pin("p") &&
            endpoint({"main", "right", "output"}) && !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            endpoint({"main", "right", "c", "p"}) && !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            occurrence_view_choice_->currentData().toStringList() == capacitor && drafts();
        const auto return_selected = endpoint({"main", "right", "r", "n"}) && inspect_peer_->isEnabled();
        inspect_peer_->click();
        checks["reverse_peer_navigation_requires_fresh_explicit_capture"] = return_selected &&
            occurrence_view_choice_->currentData().toStringList() == right && !occurrence_capture_ && selected_terminal_.isEmpty() &&
            drafts() && artwork_->capture() == library && captureOccurrence(occurrence_root) && occurrence_capture_ != before_peer;
        pin("n"); endpoint({"main", "right", "c", "p"});
        local_endpoints_->clearSelection();
        checks["cleared_peer_row_disables_and_refuses_without_rebinding"] = !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            occurrence_view_choice_->currentData().toStringList() == right && occurrence_capture_ && drafts();
        endpoint({"main", "right", "c", "p"}); pin("p");
        checks["pin_refresh_clears_peer_row_and_action"] = local_endpoints_->selectedItems().isEmpty() && !inspect_peer_->isEnabled() &&
            pin("n") && local_endpoints_->selectedItems().isEmpty() && !inspect_peer_->isEnabled() && drafts();
        choose(left); pin("n");
        const auto left_selected = endpoint({"main", "left", "c", "p"}) && inspect_peer_->isEnabled();
        inspect_peer_->click();
        checks["reused_peer_navigation_preserves_containing_occurrence_identity"] = left_selected &&
            occurrence_view_choice_->currentData().toStringList() == QStringList{"main", "left", "c"} && current_occurrence_ &&
            current_occurrence_->path == std::vector<std::string>{"main", "left", "c"} && drafts() &&
            selected_instance_ == "right" && artwork_->capture() == library && !occurrence_capture_;
        choose(right); captureOccurrence(occurrence_root); pin("n");
        const auto first = occurrence_capture_;
        views_->setCurrentIndex(1); views_->setCurrentIndex(0); views_->setCurrentIndex(1);
        catalog_->setCurrentRow(cap);
        checks["tab_and_catalog_independence"] = drafts() && occurrence_capture_ == first && !artwork_->capture() &&
            occurrence_view_choice_->currentData().toStringList() == right && selected_instance_ == "right";
        catalog_->setCurrentRow(resistor); previewArtwork(library_root);
        checks["path_switch_clears_only_occurrence_keeps_drafts"] = choose(left) && !occurrence_capture_ && artwork_->capture() &&
            drafts() && selected_instance_ == "right" && occurrence_->currentText() == "main/right";
        checks["left_occurrence_uses_correct_applied_value"] = captureOccurrence(occurrence_root) && current_occurrence_ &&
            current_occurrence_->parameters.at("resistance").value == "1000" && occurrence_view_note_->text().contains("main/left/r");
        checks["reused_local_net_ids_keep_distinct_occurrence_paths"] = terminals("main/left", "junction", "drive") && drafts();
        checks["occurrence_change_clears_pin_and_net_identity"] = selected_terminal_.isEmpty() && !current_local_net_ && local_endpoints_->rowCount() == 0 &&
            pin("n") && junction("main/left") && drafts();
        checks["return_right_requires_new_capture"] = choose(right) && !occurrence_capture_ && drafts() && captureOccurrence(occurrence_root);
        checks["unavailable_component_not_library_symbol"] = choose(capacitor) && !occurrence_capture_ && !capture_occurrence_->isEnabled() &&
            !captureOccurrence(occurrence_root) && artwork_->capture() && selectedComponent() == "resistor" && drafts();
        checks["capacitor_terminals_independent_of_unsupported_artwork"] = terminals("main/right", "return", "junction") &&
            !occurrence_capture_ && drafts();
        checks["capacitor_pin_members_without_artwork"] = selected_terminal_.isEmpty() && pin("p") && current_local_net_ &&
            current_local_net_->net.id == "junction" && local_endpoints_->rowCount() == 3 && !occurrence_capture_ && drafts();
        choose(right); captureOccurrence(occurrence_root);
        pin("n");
        const auto captured = occurrence_capture_;
        checks["failed_capture_retains_prior"] = !captureOccurrence(occurrence_root + "/missing") && occurrence_capture_ == captured &&
            occurrence_view_note_->text().contains("prior occurrence capture retained") && drafts();
        const auto source = occurrence_root + "/tests/schema/fixtures/assets/passive.svg", moved = source + ".temporarily-absent";
        const auto removed = QFile::rename(source, moved);
        occurrence_artwork_->resize(occurrence_artwork_->width(), occurrence_artwork_->height() + 1);
        settle();
        const auto image = occurrence_artwork_->grab();
        checks["repaint_with_source_absent_no_new_authority"] = removed && !QFile::exists(source) && !image.isNull() &&
            occurrence_artwork_->capture() == captured->symbol && document_.graph() == original && document_.bytes() == bytes;
        checks["restored_source_and_drafts_unchanged"] = QFile::rename(moved, source) && drafts() && occurrence_capture_ == captured;
        name_->setText(text(document_.name())); instance_name_->setText("right"); capacitance_->setText("220");
        apply_resistance_->click();
        const auto edited = document_.graph();
        checks["existing_right_edit_refreshes_selected_occurrence"] = edited != original && occurrence_capture_ == captured &&
            current_occurrence_ && current_occurrence_->parameters.at("resistance").value == "3500" &&
            occurrence_view_note_->text().contains("3500 ohm") && occurrence_view_choice_->currentData().toStringList() == right && !editsPending();
        checks["history_refreshes_current_value_keeps_capture"] = undoEdit() && occurrence_capture_ == captured &&
            current_occurrence_->parameters.at("resistance").value == "2200" && redoEdit() && document_.graph() == edited &&
            occurrence_capture_ == captured && current_occurrence_->parameters.at("resistance").value == "3500";
        structure_->setCurrentItem(instanceItem("rc", "r"));
        checks["name_refresh_ignores_label_and_offsets"] = applyInstanceName("Shown resistor") && occurrence_capture_ == captured &&
            current_occurrence_->name == "Shown resistor" && occurrence_view_note_->text().contains("Shown resistor") &&
            captured->occurrence.name == "r" && artwork_->capture();
        checks["edit_history_and_name_keep_local_membership"] = terminals("main/right", "junction", "drive") &&
            current_occurrence_->terminals.size() == 2 && captured->occurrence.terminals.front().net->id == "junction";
        checks["edits_history_name_requery_net_keep_pin_id"] = junction("main/right") && current_local_net_->endpoints.back().terminal == "n" &&
            current_local_net_->endpoints.back().name == "2" && occurrence_capture_ == captured;
        checks["restored_name_copy_retains_full_identity"] = undoEdit() && document_.graph() == edited && current_occurrence_->name == "r" &&
            saveCopy("occurrence-copy.json") && occurrence_capture_ == captured && document_.dirty() && document_.leaf() == "original.json" && document_.can_redo();
        checks["failures_retain_current_state"] = !saveCopy("occurrence-copy.json") && !openDocument(root, "invalid.json") &&
            document_.graph() == edited && occurrence_capture_ == captured && occurrence_view_note_->text().contains("Open refused") && document_.can_redo();
        checks["failed_open_copy_retain_local_membership"] = terminals("main/right", "junction", "drive");
        checks["failed_operations_keep_pin_and_members"] = junction("main/right") && selected_terminal_ == "n" && document_.graph() == edited;
        analyzer_.close(); showAnalyzer();
        checks["analyzer_reopen_preserves_occurrence"] = analyzer_.isVisible() && occurrence_capture_ == captured && document_.graph() == edited;
        checks["explicit_reopen_clears_view_and_history"] = openDocument(root, "occurrence-copy.json") && !current_occurrence_ &&
            !occurrence_capture_ && !artwork_->capture() && !document_.dirty() && !document_.can_undo() && !document_.can_redo() &&
            occurrence_terminals_->rowCount() == 0;
        const auto cleared_terminals = occurrence_terminals_->rowCount() == 0;
        const auto cleared_members = selected_terminal_.isEmpty() && !current_local_net_ && local_endpoints_->rowCount() == 0;
        checks["persisted_occurrence_reselected_and_captured"] = choose(right) && current_occurrence_ &&
            current_occurrence_->parameters.at("resistance").value == "3500" && captureOccurrence(occurrence_root);
        checks["reopen_clears_and_reselection_requeries_local_membership"] = cleared_terminals && terminals("main/right", "junction", "drive");
        checks["successful_open_clears_pin_requires_explicit_reselection"] = cleared_members && selected_terminal_.isEmpty() &&
            local_endpoints_->rowCount() == 0 && pin("n") && junction("main/right");
        const auto reopened = occurrence_capture_;
        // Harness-only requery tests binding retention; these direct native Opens
        // are not a presentation Open workflow or a user binding-edit/recovery flow.
        const auto reordered = document_.open(utf8(root), "reordered.json");
        refreshOccurrenceChoices();
        checks["reordered_definitions_and_maps_retain_capture"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(reordered) &&
            occurrence_capture_ == reopened && current_occurrence_->path == reopened->occurrence.path;
        const auto swapped = document_.open(utf8(root), "swapped.json");
        refreshOccurrenceChoices();
        checks["changed_binding_clears_occurrence_capture"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(swapped) &&
            current_occurrence_ && !occurrence_capture_ && capture_occurrence_->isEnabled();
        checks["reordered_and_swapped_symbol_map_keep_logical_membership"] = terminals("main/right", "junction", "drive");
        checks["same_ids_requery_members_across_order_and_symbol_map"] = junction("main/right") && !occurrence_capture_;
        openDocument(root, "occurrence-copy.json"); choose(right); captureOccurrence(occurrence_root);
        pin("n");
        const auto before_net_change = occurrence_capture_;
        const auto changed = document_.open(utf8(root), "changed-nets.json");
        refreshOccurrenceChoices();
        checks["changed_local_nets_refresh_without_label_rebinding_or_recapture"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(changed) &&
            terminals("main/right", "drive_alt", "junction") && occurrence_capture_ == before_net_change &&
            occurrence_terminals_->item(0, 3)->text() == QString::fromUtf8("<b>same Ω label</b>") &&
            occurrence_terminals_->item(1, 3)->text() == occurrence_terminals_->item(0, 3)->text() &&
            before_net_change->occurrence.terminals.front().net->id == "junction";
        checks["changed_membership_requeries_same_pin_without_label_rebinding"] = selected_terminal_ == "n" && current_local_net_ &&
            current_local_net_->net.id == "drive_alt" && local_endpoints_->rowCount() == 2 &&
            local_endpoints_->item(0, 3)->text() == "main/right/input" && local_endpoints_->item(1, 3)->text() == "main/right/r/n";
        const auto unconnected = document_.open(utf8(root), "unconnected.json");
        refreshOccurrenceChoices();
        checks["unconnected_pin_explicit_local_absence_keeps_artwork"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(unconnected) &&
            terminals("main/right", {}, "drive") && occurrence_capture_ == before_net_change;
        checks["unconnected_pin_clears_members_without_silent_rebinding"] = selected_terminal_ == "n" && !current_local_net_ &&
            local_endpoints_->rowCount() == 0 && local_endpoints_note_->text().contains("Unconnected locally") && occurrence_capture_ == before_net_change &&
            !inspect_peer_->isEnabled() && !inspectSelectedPeer();
        const auto removed_pin = document_.open(utf8(root), "removed-pin.json");
        refreshOccurrenceChoices();
        checks["removed_pin_clears_selection_without_rebinding_to_remaining_pin"] = std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(removed_pin) &&
            current_occurrence_ && current_occurrence_->path == std::vector<std::string>{"main", "right", "r"} &&
            occurrence_terminals_->rowCount() == 1 && occurrence_terminals_->item(0, 0)->text() == "p" &&
            selected_terminal_.isEmpty() && !current_local_net_ && local_endpoints_->rowCount() == 0 && !inspect_peer_->isEnabled();
        choose({});
        checks["absent_path_clears_terminal_view_and_occurrence_capture"] = !current_occurrence_ && !occurrence_capture_ && occurrence_terminals_->rowCount() == 0;
        checks["missing_occurrence_clears_selected_net_details"] = selected_terminal_.isEmpty() && !current_local_net_ && local_endpoints_->rowCount() == 0 &&
            !inspect_peer_->isEnabled() && !inspectSelectedPeer();
        openDocument(root, "peer-labels.json"); choose(right); pin("n");
        local_endpoints_->sortItems(3, Qt::DescendingOrder);
        const auto labels_graph = document_.graph();
        const auto labels_selected = endpoint({"main", "right", "c", "p"});
        const auto labels_plain = labels_selected && local_endpoints_->item(local_endpoints_->selectedItems().front()->row(), 1)->text() == QString::fromUtf8("<b>same Ω label</b>");
        inspect_peer_->click();
        checks["peer_ids_survive_reordered_rows_and_equal_untrusted_labels"] = labels_selected && labels_plain &&
            occurrence_view_choice_->currentData().toStringList() == capacitor && current_occurrence_->component == "capacitor" &&
            current_occurrence_->name == utf8(QString::fromUtf8("<b>same Ω label</b>")) && document_.graph() == labels_graph && !occurrence_capture_;
        openDocument(root, "peer-ports.json"); choose({"main", "probe"}); pin("p");
        checks["root_local_port_self_and_subcircuit_ports_refuse_navigation"] = current_local_net_ && local_endpoints_->rowCount() == 4 &&
            local_endpoints_->item(0, 0)->text() == "Local circuit port" && local_endpoints_->item(2, 0)->text() == "Subcircuit port" &&
            endpoint({"main", "source"}) && !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            endpoint({"main", "probe", "p"}) && !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            endpoint({"main", "left", "input"}) && !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            endpoint({"main", "right", "input"}) && !inspect_peer_->isEnabled() && !inspectSelectedPeer() &&
            occurrence_view_choice_->currentData().toStringList() == QStringList{"main", "probe"} && !document_.dirty() && !occurrence_capture_;
        openDocument(root, "original.json"); choose(right); pin("n"); captureOccurrence(occurrence_root);
        structure_->setCurrentItem(instanceItem("main", "right"));
        name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
        endpoint({"main", "right", "c", "p"});
        const auto stale_capture = occurrence_capture_;
        const auto missing_peer = document_.open(utf8(root), "missing-peer.json");
        const auto changed_graph = document_.graph();
        const auto changed_bytes = std::string(document_.bytes());
        inspect_peer_->click();
        checks["stale_removed_peer_membership_refuses_current_graph_inertly"] =
            std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(missing_peer) && !inspect_peer_->isEnabled() &&
            occurrence_view_choice_->currentData().toStringList() == right && occurrence_capture_ == stale_capture && drafts() &&
            document_.graph() == changed_graph && document_.bytes() == changed_bytes && !document_.dirty() && !document_.can_undo();
        refreshOccurrenceChoices();
        checks["requery_after_peer_removal_clears_row_without_rebinding"] = selected_terminal_ == "n" && current_local_net_ &&
            local_endpoints_->rowCount() == 2 && local_endpoints_->selectedItems().isEmpty() && !inspect_peer_->isEnabled() &&
            !endpoint({"main", "right", "c", "p"}) && occurrence_capture_ == stale_capture && drafts();
        openDocument(root, "original.json"); choose(right);
        structure_->setCurrentItem(instanceItem("main", "right"));
        const auto history_original = document_.graph();
        const auto history_ready = applyResistance("3.5") && undoEdit() && redoEdit() && pin("n") &&
            captureOccurrence(occurrence_root) && endpoint({"main", "right", "c", "p"});
        const auto history_graph = document_.graph();
        const auto history_bytes = std::string(document_.bytes());
        inspect_peer_->click();
        checks["peer_navigation_preserves_applied_revision_history_and_save_association"] = history_ready &&
            occurrence_view_choice_->currentData().toStringList() == capacitor && document_.graph() == history_graph &&
            document_.bytes() == history_bytes && document_.dirty() && document_.can_undo() && !document_.can_redo() &&
            document_.leaf() == "original.json" && document_.root() == utf8(root) && selected_instance_ == "right" && !occurrence_capture_;
        checks["history_requery_keeps_peer_destination_without_recapture"] = undoEdit() && document_.graph() == history_original &&
            occurrence_view_choice_->currentData().toStringList() == capacitor && redoEdit() && document_.graph() == history_graph &&
            occurrence_view_choice_->currentData().toStringList() == capacitor && !occurrence_capture_ && selected_terminal_.isEmpty();
        openDocument(root, "occurrence-copy.json"); choose(right); captureOccurrence(occurrence_root);
        pin("n");
        views_->setCurrentIndex(1);
        catalog_->setCurrentRow(resistor); previewArtwork(library_root);
        structure_->setCurrentItem(instanceItem("main", "right"));
        endpoint({"main", "right", "c", "p"});
        settle();
        checks["final_caption_and_control_complete"] = occurrence_view_note_->textFormat() == Qt::PlainText &&
            occurrence_view_note_->text().contains("main/right/r") && occurrence_view_note_->text().contains("3500 ohm") &&
            occurrence_view_note_->text().contains("no saved position") && occurrence_view_note_->height() >= occurrence_view_note_->heightForWidth(occurrence_view_note_->width()) &&
            views_->currentWidget()->rect().contains(QRect(capture_occurrence_->mapTo(views_->currentWidget(), QPoint{}), capture_occurrence_->size())) &&
            views_->currentWidget()->rect().contains(QRect(occurrence_terminals_->mapTo(views_->currentWidget(), QPoint{}), occurrence_terminals_->size())) &&
            terminals("main/right", "junction", "drive") &&
            grab().save(report + ".png");
        checks["final_member_table_and_scope_complete"] = junction("main/right") &&
            local_endpoints_note_->text().contains("Ports are not traversed") &&
            local_endpoints_note_->height() >= local_endpoints_note_->heightForWidth(local_endpoints_note_->width()) &&
            views_->currentWidget()->rect().contains(QRect(local_endpoints_->mapTo(views_->currentWidget(), QPoint{}), local_endpoints_->size())) &&
            local_endpoints_->viewport()->rect().contains(local_endpoints_->visualItemRect(local_endpoints_->item(2, 3)));
        checks["final_peer_action_complete_and_enabled_for_declared_peer"] = inspect_peer_->isEnabled() &&
            views_->currentWidget()->rect().contains(QRect(inspect_peer_->mapTo(views_->currentWidget(), QPoint{}), inspect_peer_->size())) &&
            peerOccurrencePath() == capacitor;
    }
    QJsonArray terminal_rows;
    for(int row = 0; row < occurrence_terminals_->rowCount(); ++row) {
        QJsonArray cells;
        for(int column = 0; column < 5; ++column) cells.append(occurrence_terminals_->item(row, column)->text());
        terminal_rows.append(cells);
    }
    QJsonArray endpoint_rows;
    for(int row = 0; row < local_endpoints_->rowCount(); ++row) {
        QJsonArray cells;
        for(int column = 0; column < 4; ++column) cells.append(local_endpoints_->item(row, column)->text());
        endpoint_rows.append(cells);
    }
    bool passed = checks.size() == 67;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"occurrence_note", occurrence_view_note_->text()}, {"terminal_rows", terminal_rows}, {"endpoint_rows", endpoint_rows},
        {"local_endpoints_note", local_endpoints_note_->text()},
        {"peer_action_enabled", inspect_peer_->isEnabled()}, {"peer_destination", peerOccurrencePath().join("/")},
        {"scope", "explicit direct peer-component navigation by kind/full IDs; no port traversal/flattening/auto-pin/resource capture; four drafts and independent views/history/copy preserved; no geometry or human recovery acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}
