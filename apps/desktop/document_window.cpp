// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "document_window.hpp"
#include "preview_canvas.hpp"
#include "rc_canvas.hpp"
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
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QWheelEvent>
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
#include <QToolBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <limits>
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
    resize(1280, 800);
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
    declaration_panel_ = central;
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
    resistance_details_host_ = new QWidget;
    auto* resistance_host_layout = new QVBoxLayout(resistance_details_host_);
    resistance_host_layout->setContentsMargins(0, 0, 0, 0);
    instance_layout->addWidget(resistance_details_host_);
    resistance_editor_ = new QWidget;
    auto* resistance_layout = new QVBoxLayout(resistance_editor_);
    resistance_layout->setContentsMargins(0, 0, 0, 0);
    resistance_host_layout->addWidget(resistance_editor_);
    resistance_limits_ = new QLabel("Select an existing literal resistance override.");
    resistance_limits_->setWordWrap(true);
    resistance_limits_->setTextFormat(Qt::PlainText);
    resistance_layout->addWidget(resistance_limits_);
    resistance_ = new QLineEdit;
    resistance_->setAccessibleName("Declared literal resistance value");
    resistance_->setMaxLength(64);
    resistance_layout->addWidget(resistance_);
    apply_resistance_ = new QPushButton("Apply Resistance Value");
    resistance_layout->addWidget(apply_resistance_);
    capacitance_details_host_ = new QWidget;
    auto* capacitance_host_layout = new QVBoxLayout(capacitance_details_host_);
    capacitance_host_layout->setContentsMargins(0, 0, 0, 0);
    instance_layout->addWidget(capacitance_details_host_);
    capacitance_editor_ = new QWidget;
    auto* capacitance_layout = new QVBoxLayout(capacitance_editor_);
    capacitance_layout->setContentsMargins(0, 0, 0, 0);
    capacitance_host_layout->addWidget(capacitance_editor_);
    capacitance_limits_ = new QLabel("Select an existing literal capacitance override.");
    capacitance_limits_->setWordWrap(true);
    capacitance_limits_->setTextFormat(Qt::PlainText);
    capacitance_layout->addWidget(capacitance_limits_);
    capacitance_ = new QLineEdit;
    capacitance_->setAccessibleName("Declared literal capacitance value");
    capacitance_->setMaxLength(64);
    capacitance_layout->addWidget(capacitance_);
    apply_capacitance_ = new QPushButton("Apply Capacitance Value");
    capacitance_layout->addWidget(apply_capacitance_);
    for(auto* label : {inspector_, occurrence_note_, definition_note, resistance_limits_, capacitance_limits_})
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    instance_layout->addStretch();
    properties_scroll_ = new QScrollArea;
    properties_scroll_->setWidgetResizable(true);
    properties_scroll_->setWidget(instance_panel);
    properties_->setWidget(properties_scroll_);
    declaration_properties_ = properties_scroll_;
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
    details_action_ = view->addAction("Declaration Details");
    details_action_->setCheckable(true);
    connect(details_action_, &QAction::toggled, this, &DocumentWindow::showDeclarationDetails);
    focus_action_ = view->addAction("Focus Circuit");
    focus_action_->setCheckable(true);
    focus_action_->setToolTip("Temporarily hide Components/Preview and Properties; toggle again to restore their layout.");
    connect(focus_action_, &QAction::toggled, this, [this](bool enabled) { setCircuitFocus(enabled); });
    auto* toolbar = addToolBar("Document");
    toolbar->setObjectName("document-actions");
    open->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    copy->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    toolbar->addAction(open);
    toolbar->addAction(copy);
    toolbar->addSeparator();
    toolbar->addAction(undo_);
    toolbar->addAction(redo_);
    toolbar->addSeparator();
    toolbar->addAction(details_action_);
    toolbar->addAction(focus_action_);
    connect(apply_, &QPushButton::clicked, this, [this] { applyName(name_->text()); });
    connect(apply_instance_, &QPushButton::clicked, this, [this] { applyInstanceName(instance_name_->text()); });
    connect(apply_resistance_, &QPushButton::clicked, this, [this] {
        if(details_action_->isChecked()) applyResistance(resistance_->text());
        else applyCircuitResistance();
    });
    connect(apply_capacitance_, &QPushButton::clicked, this, [this] {
        if(details_action_->isChecked()) applyCapacitance(capacitance_->text());
        else applyCircuitCapacitance();
    });
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
    circuit_panel_ = new QWidget;
    auto* circuit_layout = new QVBoxLayout(circuit_panel_);
    circuit_context_ = new QComboBox;
    circuit_context_->setAccessibleName("RC occurrence shown in the circuit canvas");
    circuit_context_->addItem("RC instance: right", QStringList{"main", "right"});
    circuit_context_->addItem("RC instance: left", QStringList{"main", "left"});
    auto* navigation = new QHBoxLayout;
    navigation->addWidget(circuit_context_, 1);
    circuit_zoom_out_ = new QPushButton("Zoom Out");
    circuit_fit_ = new QPushButton("Fit");
    circuit_zoom_in_ = new QPushButton("Zoom In");
    circuit_zoom_note_ = new QLabel;
    circuit_zoom_note_->setToolTip("Scale relative to Fit; changes only this view.");
    navigation->addWidget(circuit_zoom_out_);
    navigation->addWidget(circuit_fit_);
    navigation->addWidget(circuit_zoom_in_);
    navigation->addWidget(circuit_zoom_note_);
    circuit_layout->addLayout(navigation);
    circuit_ = new RcCanvas;
    circuit_->zoomed = [this] { refreshCircuit(); };
    circuit_layout->addWidget(circuit_, 1);
    connect(circuit_zoom_in_, &QPushButton::clicked, this, [this] { circuit_->zoomIn(); refreshCircuit(); });
    connect(circuit_zoom_out_, &QPushButton::clicked, this, [this] { circuit_->zoomOut(); refreshCircuit(); });
    connect(circuit_fit_, &QPushButton::clicked, this, [this] { circuit_->fitView(); refreshCircuit(); });
    auto* circuit_note = new QLabel("Double-click R/C: edit · Canvas focus: Enter edits selection, PgUp/PgDn zoom, Home fits · Wheel: zoom · Middle-drag: pan · simulation unavailable");
    circuit_note->setWordWrap(true);
    circuit_layout->addWidget(circuit_note);
    circuit_properties_ = new QWidget;
    auto* selection_layout = new QVBoxLayout(circuit_properties_);
    circuit_selection_ = new QLabel;
    circuit_selection_->setTextFormat(Qt::PlainText);
    circuit_selection_->setWordWrap(true);
    circuit_selection_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    selection_layout->addWidget(circuit_selection_);
    circuit_edit_ = new QPushButton("Edit containing RC instance...");
    selection_layout->addWidget(circuit_edit_);
    circuit_resistance_edit_ = new QPushButton("Edit Resistance");
    selection_layout->addWidget(circuit_resistance_edit_);
    circuit_resistance_target_note_ = new QLabel;
    circuit_resistance_target_note_->setTextFormat(Qt::PlainText);
    circuit_resistance_target_note_->setWordWrap(true);
    circuit_resistance_target_note_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    selection_layout->addWidget(circuit_resistance_target_note_);
    resistance_circuit_host_ = new QWidget;
    auto* circuit_resistance_layout = new QVBoxLayout(resistance_circuit_host_);
    circuit_resistance_layout->setContentsMargins(0, 0, 0, 0);
    selection_layout->addWidget(resistance_circuit_host_);
    circuit_capacitance_edit_ = new QPushButton("Edit Capacitance");
    selection_layout->addWidget(circuit_capacitance_edit_);
    circuit_capacitance_target_note_ = new QLabel;
    circuit_capacitance_target_note_->setTextFormat(Qt::PlainText);
    circuit_capacitance_target_note_->setWordWrap(true);
    circuit_capacitance_target_note_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    selection_layout->addWidget(circuit_capacitance_target_note_);
    capacitance_circuit_host_ = new QWidget;
    auto* circuit_capacitance_layout = new QVBoxLayout(capacitance_circuit_host_);
    circuit_capacitance_layout->setContentsMargins(0, 0, 0, 0);
    selection_layout->addWidget(capacitance_circuit_host_);
    selection_layout->addStretch();
    connect(circuit_context_, &QComboBox::currentIndexChanged, this, [this] { refreshCircuit(); });
    connect(circuit_edit_, &QPushButton::clicked, this, [this] { editContainingRc(); });
    connect(circuit_resistance_edit_, &QPushButton::clicked, this, [this] { beginCircuitResistanceEdit(); });
    connect(circuit_capacitance_edit_, &QPushButton::clicked, this, [this] { beginCircuitCapacitanceEdit(); });
    circuit_->selected = [this](const QStringList& path) {
        const auto index = occurrence_view_choice_->findData(path);
        if(index < 0) return;
        occurrence_view_choice_->setCurrentIndex(index);
        refreshCircuit();
    };
    circuit_->activated = [this](const QStringList& path) {
        if(details_action_->isChecked() || path.size() != 3 || path != circuit_->selection()) return;
        const auto is_resistance = path.back() == "r";
        if(!is_resistance && path.back() != "c") return;
        if(!(is_resistance ? beginCircuitResistanceEdit() : beginCircuitCapacitanceEdit())) return;
        if(circuit_focused_ && !setCircuitFocus(false)) return;
        properties_->show();
        auto* field = is_resistance ? resistance_ : capacitance_;
        field->setFocus(Qt::MouseFocusReason);
        field->selectAll();
    };
    showDeclarationDetails(false);
    refresh();
}

bool DocumentWindow::setCircuitFocus(bool enabled)
{
    const QSignalBlocker blocked(focus_action_);
    focus_action_->setChecked(circuit_focused_);
    if(enabled == circuit_focused_) return true;
    if(enabled) {
        if(details_action_->isChecked()) return false;
        focus_layout_ = saveState(1);
        focus_splitter_ = catalog_splitter_->saveState();
        if(focus_layout_.isEmpty() || focus_splitter_.isEmpty()) return false;
        components_->hide();
        properties_->hide();
    } else if(!restoreState(focus_layout_, 1) || !catalog_splitter_->restoreState(focus_splitter_)) {
        components_->hide();
        properties_->hide();
        return false;
    }
    circuit_focused_ = enabled;
    focus_action_->setChecked(enabled);
    focus_action_->setText(enabled ? "Restore Panels" : "Focus Circuit");
    components_->toggleViewAction()->setEnabled(!enabled);
    properties_->toggleViewAction()->setEnabled(!enabled);
    details_action_->setEnabled(!enabled);
    if(!enabled) { focus_layout_.clear(); focus_splitter_.clear(); }
    return true;
}
void DocumentWindow::showDeclarationDetails(bool enabled)
{
    if(circuit_focused_ && enabled) return;
    auto* target = enabled ? declaration_panel_ : circuit_panel_;
    auto* properties = enabled ? declaration_properties_ : circuit_properties_;
    if(!target || !properties) return;
    if(centralWidget() != target) {
        auto* old = takeCentralWidget();
        if(old) { old->hide(); old->setParent(this); }
        setCentralWidget(target);
        target->show();
    }
    if(properties_->widget() != properties) {
        auto* old = properties_->widget();
        if(old) { old->hide(); old->setParent(this); }
        properties_->setWidget(properties);
        properties->show();
    }
    const QSignalBlocker blocked(details_action_);
    details_action_->setChecked(enabled);
    focus_action_->setEnabled(!enabled);
    // Transfer the existing editor, not a copied value or a second draft.
    auto* host = enabled ? capacitance_details_host_ : capacitance_circuit_host_;
    if(capacitance_editor_->parentWidget() != host) host->layout()->addWidget(capacitance_editor_);
    capacitance_editor_->show();
    auto* resistance_host = enabled ? resistance_details_host_ : resistance_circuit_host_;
    if(resistance_editor_->parentWidget() != resistance_host) resistance_host->layout()->addWidget(resistance_editor_);
    resistance_editor_->show();
    refreshCircuit();
}
void DocumentWindow::refreshCircuit()
{
    if(!circuit_) return;
    const auto context = circuit_context_->currentData().toStringList();
    circuit_->setGraph(document_.graph(), context);
    circuit_zoom_in_->setEnabled(circuit_->supported() && circuit_->zoomPercent() < 150);
    circuit_zoom_out_->setEnabled(circuit_->supported() && circuit_->zoomPercent() > 50);
    circuit_fit_->setEnabled(circuit_->supported());
    circuit_zoom_note_->setText("Zoom: " + QString::number(circuit_->zoomPercent()) + "%");
    QStringList path;
    if(current_occurrence_) for(const auto& id : current_occurrence_->path) path << text(id);
    circuit_->setSelection(path);
    circuit_edit_->setEnabled(!circuit_->selection().isEmpty());
    const auto eligible = circuitCapacitanceEligible();
    circuit_capacitance_edit_->setEnabled(eligible);
    const auto active = eligible && circuit_capacitance_target_ == context &&
        selected_circuit_ == context[0] && selected_instance_ == context[1];
    const auto circuit_view = !details_action_->isChecked();
    const auto resistance_eligible = circuitResistanceEligible();
    circuit_resistance_edit_->setEnabled(resistance_eligible);
    const auto resistance_active = resistance_eligible && circuit_resistance_target_ == context &&
        selected_circuit_ == context[0] && selected_instance_ == context[1];
    resistance_circuit_host_->setVisible(circuit_view && resistance_active);
    resistance_circuit_host_->setEnabled(resistance_active);
    circuit_resistance_target_note_->setVisible(circuit_view && resistance_active);
    if(resistance_active) {
        const auto literal = selectedResistance();
        circuit_resistance_target_note_->setText("Editing resistance of containing RC " + context.join("/") +
            "\nValue in " + text(literal->unit) + "; unit fixed. Apply changes the declaration.");
    }
    capacitance_circuit_host_->setVisible(circuit_view && active);
    capacitance_circuit_host_->setEnabled(active);
    circuit_capacitance_target_note_->setVisible(circuit_view && active);
    if(active) {
        const auto literal = selectedCapacitance();
        circuit_capacitance_target_note_->setText("Editing capacitance of containing RC " + context.join("/") +
            "\nValue in " + text(literal->unit) + "; unit fixed. Apply changes the declaration.");
    }
    if(circuit_->selection().isEmpty()) {
        circuit_selection_->setText("Click a resistor or capacitor in the circuit to inspect its applied value.");
        return;
    }
    const auto& component = *current_occurrence_;
    const auto parameter_id = component.component == "resistor" ? "resistance" : "capacitance";
    const auto& parameter = component.parameters.at(parameter_id);
    circuit_selection_->setText(text(component.name) + "\n" + text(component.component) + "\n\nOccurrence: " + path.join("/") +
        "\n\nApplied " + parameter_id + ":\n" + RcCanvas::parameterCaption(parameter) +
        "\nBase value: " + text(parameter.value) + " " + text(parameter.unit) +
        "\n\nBinding origin:\n" + text(parameter.origin) + "\n\nEdit target: containing RC instance " + context.join("/") +
        "\nThe component receives this parameter from its declaration binding.");
}
bool DocumentWindow::circuitCapacitanceEligible() const
{
    const auto context = circuit_context_->currentData().toStringList();
    return document_.graph() && circuit_->supported() && context.size() == 2 &&
        circuit_->selection() == QStringList{context[0], context[1], "c"} &&
        simnodus::literal_capacitance(*document_.graph(), utf8(context[0]), utf8(context[1])).has_value();
}
bool DocumentWindow::beginCircuitCapacitanceEdit()
{
    refreshCircuit();
    if(!circuitCapacitanceEligible()) {
        status_->setText("Select a supported RC capacitor with an existing containing capacitance value.");
        return false;
    }
    const auto context = circuit_context_->currentData().toStringList();
    if((selected_circuit_ != context[0] || selected_instance_ != context[1]) &&
        (instanceDraftPending() || resistanceDraftPending() || capacitanceDraftPending())) {
        status_->setText("Apply or restore pending instance fields before editing another RC instance.");
        return false;
    }
    auto* target = instanceItem(context[0], context[1]);
    if(!target) return false;
    structure_->setCurrentItem(target);
    if(selected_circuit_ != context[0] || selected_instance_ != context[1] || !selectedCapacitance()) return false;
    circuit_capacitance_target_ = context;
    refreshCircuit();
    capacitance_->setFocus();
    status_->setText("Capacitance edit active for containing RC " + context.join("/") + "; Apply and Save Copy remain explicit.");
    return true;
}
bool DocumentWindow::applyCircuitCapacitance()
{
    refreshCircuit();
    const auto context = circuit_context_->currentData().toStringList();
    if(details_action_->isChecked() || !circuitCapacitanceEligible() || circuit_capacitance_target_ != context ||
        selected_circuit_ != context[0] || selected_instance_ != context[1]) {
        status_->setText("Return to the selected capacitor and activate its containing RC capacitance edit before applying.");
        return false;
    }
    return applyCapacitance(capacitance_->text());
}
bool DocumentWindow::circuitResistanceEligible() const
{
    const auto context = circuit_context_->currentData().toStringList();
    return document_.graph() && circuit_->supported() && context.size() == 2 &&
        circuit_->selection() == QStringList{context[0], context[1], "r"} &&
        simnodus::literal_resistance(*document_.graph(), utf8(context[0]), utf8(context[1])).has_value();
}
bool DocumentWindow::beginCircuitResistanceEdit()
{
    refreshCircuit();
    if(!circuitResistanceEligible()) {
        status_->setText("Select a supported RC resistor with an existing containing resistance value.");
        return false;
    }
    const auto context = circuit_context_->currentData().toStringList();
    if((selected_circuit_ != context[0] || selected_instance_ != context[1]) &&
        (instanceDraftPending() || resistanceDraftPending() || capacitanceDraftPending())) {
        status_->setText("Apply or restore pending instance fields before editing another RC instance.");
        return false;
    }
    auto* target = instanceItem(context[0], context[1]);
    if(!target) return false;
    structure_->setCurrentItem(target);
    if(selected_circuit_ != context[0] || selected_instance_ != context[1] || !selectedResistance()) return false;
    circuit_resistance_target_ = context;
    refreshCircuit();
    resistance_->setFocus();
    status_->setText("Resistance edit active for containing RC " + context.join("/") + "; Apply and Save Copy remain explicit.");
    return true;
}
bool DocumentWindow::applyCircuitResistance()
{
    refreshCircuit();
    const auto context = circuit_context_->currentData().toStringList();
    if(details_action_->isChecked() || !circuitResistanceEligible() || circuit_resistance_target_ != context ||
        selected_circuit_ != context[0] || selected_instance_ != context[1]) {
        status_->setText("Return to the selected resistor and activate its containing RC resistance edit before applying.");
        return false;
    }
    return applyResistance(resistance_->text());
}
bool DocumentWindow::editContainingRc()
{
    refreshCircuit();
    if(circuit_->selection().isEmpty()) return false;
    const auto context = circuit_context_->currentData().toStringList();
    if((selected_circuit_ != context[0] || selected_instance_ != context[1]) &&
        (instanceDraftPending() || resistanceDraftPending() || capacitanceDraftPending())) {
        status_->setText("Apply or restore pending instance fields before editing another RC instance.");
        return false;
    }
    auto* target = instanceItem(context[0], context[1]);
    if(!target) return false;
    structure_->setCurrentItem(target);
    views_->setCurrentIndex(0);
    showDeclarationDetails(true);
    status_->setText("Editing the containing RC instance " + context.join("/") + "; component parameter bindings remain unchanged.");
    return selected_circuit_ == context[0] && selected_instance_ == context[1];
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
    if(circuit_) circuit_->fitView();
    circuit_capacitance_target_.clear();
    circuit_resistance_target_.clear();
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
    refreshCircuit();
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
        if(const auto parameter = current.parameters.find("capacitance"); parameter != current.parameters.end() && parameter->second.unit == "F")
            note += "\nApplied capacitance: " + text(parameter->second.value) + " F (" + text(parameter->second.origin) + ")";
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    showDeclarationDetails(true);
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
    const auto retained_occurrence_note = occurrence_view_note_->text();
    const auto retained_endpoints_note = local_endpoints_note_->text();
    const auto retained_peer_enabled = inspect_peer_->isEnabled();
    const auto retained_peer_destination = peerOccurrencePath().join("/");
    QJsonArray capacitance_observations;
    const auto observe_capacitance = [this, &capacitance_observations](const QString& stage) {
        if(!current_occurrence_ || !current_occurrence_->parameters.contains("capacitance")) return;
        const auto& parameter = current_occurrence_->parameters.at("capacitance");
        QJsonArray path;
        for(const auto& id : current_occurrence_->path) path.append(text(id));
        capacitance_observations.append(QJsonObject{{"stage", stage}, {"path", path}, {"value", text(parameter.value)},
            {"unit", text(parameter.unit)}, {"origin", text(parameter.origin)}, {"caption", occurrence_view_note_->text()}});
    };
    const auto capacitance_note = [this](const QString& value, const QString& origin = "containing-circuit:capacitance") {
        return current_occurrence_ && current_occurrence_->component == "capacitor" && occurrence_view_note_->textFormat() == Qt::PlainText &&
            occurrence_view_note_->text().contains("Applied capacitance: " + value + " F (" + origin + ")") &&
            occurrence_view_note_->text().contains("no saved position or runtime measurement");
    };
    if(openDocument(root, "original.json")) {
        choose(capacitor);
        const auto before_c = document_.graph();
        checks["current_capacitance_available_without_capacitor_artwork"] = capacitance_note("0.000000220") &&
            !capture_occurrence_->isEnabled() && !occurrence_capture_ && !artwork_->capture() && !document_.dirty();
        observe_capacitance("original_right");
        structure_->setCurrentItem(instanceItem("main", "right"));
        catalog_->setCurrentRow(0); previewArtwork(library_root);
        const auto c_library = artwork_->capture();
        name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
        const auto c_drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
            resistance_->text() == "3.5" && capacitance_->text() == "470"; };
        choose({"main", "left", "c"});
        checks["capacitance_context_switch_preserves_four_drafts_and_independent_views"] = capacitance_note("0.000001") && c_drafts() &&
            artwork_->capture() == c_library && c_library && selected_instance_ == "right" && occurrence_->currentText() == "main/right" &&
            document_.graph() == before_c && !document_.dirty() && analyzer_.isVisible();
        observe_capacitance("original_left");
        choose(capacitor);
        checks["pending_capacitance_text_does_not_change_applied_caption_or_save"] = capacitance_note("0.000000220") &&
            c_drafts() && !saveCopy("pending-capacitance.json") && !undoEdit() && document_.graph() == before_c;
        observe_capacitance("draft");
        name_->setText(text(document_.name())); instance_name_->setText("right"); resistance_->setText("2.2");
        pin("p");
        apply_capacitance_->click();
        const auto applied_c = document_.graph();
        checks["capacitance_apply_requeries_current_graph_and_preserves_pin"] = capacitance_note("0.000000470") &&
            applied_c != before_c && document_.dirty() && document_.can_undo() && !editsPending() && selected_terminal_ == "p" &&
            current_local_net_ && current_local_net_->net.path == std::vector<std::string>{"main", "right", "junction"} &&
            artwork_->capture() == c_library && !occurrence_capture_;
        observe_capacitance("applied");
        checks["invalid_capacitance_edit_retains_applied_view_and_identity"] = !applyCapacitance("0") &&
            document_.graph() == applied_c && capacitance_note("0.000000470") && selected_terminal_ == "p" && document_.can_undo();
        checks["capacitance_undo_refreshes_applied_value_keeps_pin_and_library"] = undoEdit() && document_.graph() == before_c &&
            capacitance_note("0.000000220") && selected_terminal_ == "p" && artwork_->capture() == c_library && !occurrence_capture_;
        observe_capacitance("undo");
        checks["capacitance_redo_refreshes_applied_value_keeps_pin_and_library"] = redoEdit() && document_.graph() == applied_c &&
            capacitance_note("0.000000470") && selected_terminal_ == "p" && artwork_->capture() == c_library;
        observe_capacitance("redo");
        choose({"main", "left", "c"});
        checks["right_capacitance_edit_preserves_left_applied_value"] = capacitance_note("0.000001") &&
            document_.graph() == applied_c && selected_instance_ == "right" && capacitance_->text() == "470";
        observe_capacitance("left_after");
        choose(capacitor); pin("p");
        checks["capacitance_copy_and_failed_operations_keep_revision_and_association"] = saveCopy("capacitance-copy.json") &&
            !saveCopy("capacitance-copy.json") && !openDocument(root, "invalid.json") && document_.graph() == applied_c &&
            capacitance_note("0.000000470") && selected_terminal_ == "p" && document_.leaf() == "original.json" &&
            document_.dirty() && document_.can_undo() && artwork_->capture() == c_library;
        const auto reopened_c = openDocument(root, "capacitance-copy.json") && !current_occurrence_ && selected_terminal_.isEmpty() &&
            !document_.dirty() && !document_.can_undo() && !document_.can_redo() && !artwork_->capture();
        choose(capacitor); pin("p");
        checks["capacitance_copy_reopen_requires_explicit_occurrence_and_pin"] = reopened_c && capacitance_note("0.000000470") &&
            selected_terminal_ == "p" && !occurrence_capture_ && document_.leaf() == "capacitance-copy.json";
        observe_capacitance("reopen");
        openDocument(root, "wrong-capacitance-dimension.json"); choose(capacitor);
        structure_->setCurrentItem(instanceItem("main", "right"));
        checks["wrong_dimension_never_gets_capacitance_F_caption_or_edit"] = current_occurrence_ &&
            current_occurrence_->parameters.at("capacitance").unit == "s" && !occurrence_view_note_->text().contains("Applied capacitance:") &&
            !apply_capacitance_->isEnabled() && !document_.dirty() && !occurrence_capture_;
        observe_capacitance("wrong_dimension");
        openDocument(root, "default-capacitance.json"); choose(capacitor);
        checks["default_capacitance_uses_own_applied_value_and_origin"] = capacitance_note("0.000001", "default") &&
            current_occurrence_->parameters.at("capacitance").origin == "default" && !occurrence_capture_ && !document_.dirty();
        observe_capacitance("default_origin");
        openDocument(root, "capacitance-copy.json"); choose(capacitor); pin("p");
        structure_->setCurrentItem(instanceItem("main", "right"));
        catalog_->setCurrentRow(0); previewArtwork(library_root);
        views_->setCurrentIndex(1); settle();
        checks["final_capacitance_caption_and_controls_complete"] = capacitance_note("0.000000470") &&
            occurrence_view_note_->height() >= occurrence_view_note_->heightForWidth(occurrence_view_note_->width()) &&
            views_->currentWidget()->rect().contains(QRect(capture_occurrence_->mapTo(views_->currentWidget(), QPoint{}), capture_occurrence_->size())) &&
            !capture_occurrence_->isEnabled() && !occurrence_capture_ &&
            grab().save(report + ".capacitance.png");
        observe_capacitance("final");
    }
    bool passed = checks.size() == 80;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"occurrence_note", retained_occurrence_note}, {"terminal_rows", terminal_rows}, {"endpoint_rows", endpoint_rows},
        {"local_endpoints_note", retained_endpoints_note},
        {"peer_action_enabled", retained_peer_enabled}, {"peer_destination", retained_peer_destination},
        {"capacitance_observations", capacitance_observations}, {"capacitance_note", occurrence_view_note_->text()},
        {"retained_snapshot", "Existing R-only controls/rows/note/peer action and report.png precede separate C-only path/report.capacitance.png"},
        {"scope", "current owned applied capacitance and immediate origin with existing C edit/history/create-only copy; retained peer/R-only path; no measurement/model truth/automatic resource access/general properties or human recovery acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runCanvasAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto observe = [&](const QString& stage, const QString& input) {
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()}});
    };
    const auto click = [&](const QString& id) {
        const auto point = circuit_->componentPoint(id);
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(point), QPointF(circuit_->mapToGlobal(point)),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event);
        settle();
        return circuit_->selection() == QStringList{"main", circuit_context_->currentData().toStringList()[1], id};
    };
    const auto choose_context = [&](const QString& side) {
        circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main", side}));
        settle();
    };
    checks["default_canvas_without_document_or_selection"] = centralWidget() == circuit_panel_ && !circuit_->supported() &&
        circuit_->selection().isEmpty() && properties_->widget() == circuit_properties_ && !circuit_edit_->isEnabled();
    checks["inert_open_supported_exact_shape"] = openDocument(root, "original.json") && circuit_->supported() &&
        !document_.dirty() && !current_occurrence_ && !artwork_->capture() && !occurrence_capture_;
    settle();
    checks["canvas_mode_hides_declaration_fields"] = circuit_->isVisible() && !name_->isVisible() &&
        !properties_scroll_->isVisible() && !details_action_->isChecked();
    observe("original", "original.json");
    const auto original = document_.graph();
    checks["resistor_click_inspects_full_ids_without_edit_target"] = click("r") && current_occurrence_ &&
        current_occurrence_->path == std::vector<std::string>{"main", "right", "r"} && selected_instance_.isEmpty() && document_.graph() == original;
    observe("right_r", "original.json");
    checks["capacitor_click_inspects_without_capture_or_library_selection"] = click("c") && current_occurrence_ &&
        current_occurrence_->path == std::vector<std::string>{"main", "right", "c"} && !artwork_->capture() && !occurrence_capture_ &&
        catalog_->currentRow() == -1 && document_.graph() == original && circuit_selection_->textFormat() == Qt::PlainText;
    observe("right_c", "original.json");
    const auto selected = circuit_->snapshot();
    QMouseEvent blank(QEvent::MouseButtonPress, QPointF(4, 4), QPointF(circuit_->mapToGlobal(QPoint(4, 4))),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(circuit_, &blank);
    checks["blank_click_does_not_create_or_rebind"] = circuit_->snapshot() == selected && document_.graph() == original;
    resize(1430, 850); settle();
    checks["fitted_resize_hit_test_uses_same_component_identity"] = click("r") && click("c") && document_.graph() == original;
    checks["initial_canvas_image"] = grab().save(report + ".initial.png");
    checks["explicit_containing_action_uses_existing_forwarded_target"] = editContainingRc() &&
        centralWidget() == declaration_panel_ && selected_circuit_ == "main" && selected_instance_ == "right" &&
        capacitance_->text() == "220" && current_occurrence_->source_circuit == "rc" && current_occurrence_->source_instance == "c" &&
        current_occurrence_->parameters.at("capacitance").origin == "containing-circuit:capacitance" && document_.graph() == original;
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    showDeclarationDetails(false);
    checks["canvas_clicks_preserve_four_drafts_and_edit_target"] = click("r") && click("c") && drafts() &&
        document_.graph() == original && selected_instance_ == "right";
    choose_context("left"); click("c");
    checks["context_inspection_preserves_drafts_and_refuses_target_replacement"] = drafts() && !editContainingRc() &&
        selected_instance_ == "right" && centralWidget() == circuit_panel_ && document_.graph() == original;
    choose_context("right"); click("c");
    checks["pending_text_does_not_apply_or_allow_copy_history"] = drafts() && !saveCopy("pending-canvas.json") && !undoEdit() &&
        document_.graph() == original && current_occurrence_->parameters.at("capacitance").value == "0.000000220";
    observe("draft", "original.json");
    name_->setText(text(document_.name())); instance_name_->setText("right"); resistance_->setText("2.2");
    showDeclarationDetails(true);
    apply_capacitance_->click();
    const auto applied = document_.graph();
    showDeclarationDetails(false);
    checks["existing_C_apply_refreshes_canvas_and_selection"] = applied != original && document_.dirty() && !editsPending() &&
        circuit_->selection() == QStringList{"main", "right", "c"} && current_occurrence_->parameters.at("capacitance").value == "0.000000470";
    observe("applied", "canvas-copy.json");
    checks["invalid_C_edit_retains_current_graph_and_canvas"] = !applyCapacitance("0") && document_.graph() == applied &&
        circuit_->selection() == QStringList{"main", "right", "c"} && document_.can_undo();
    checks["native_undo_refreshes_canvas"] = undoEdit() && document_.graph() == original && circuit_->supported() &&
        current_occurrence_->parameters.at("capacitance").value == "0.000000220";
    observe("undo", "original.json");
    checks["native_redo_refreshes_canvas"] = redoEdit() && document_.graph() == applied && circuit_->supported() &&
        current_occurrence_->parameters.at("capacitance").value == "0.000000470";
    observe("redo", "canvas-copy.json");
    choose_context("left");
    checks["left_context_applied_value_is_independent"] = click("c") && current_occurrence_->parameters.at("capacitance").value == "0.000001" &&
        selected_instance_ == "right" && capacitance_->text() == "470" && document_.graph() == applied;
    observe("left", "canvas-copy.json");
    choose_context("right"); click("c");
    checks["create_only_copy_retains_original_association_and_history"] = saveCopy("canvas-copy.json") && document_.leaf() == "original.json" &&
        document_.dirty() && document_.can_undo() && document_.graph() == applied;
    checks["occupied_copy_refused_without_canvas_change"] = !saveCopy("canvas-copy.json") && document_.graph() == applied && circuit_->supported();
    const auto before_failed_open = circuit_->snapshot();
    checks["failed_open_preserves_current_inspection_and_revision"] = !openDocument(root, "invalid.json") &&
        document_.graph() == applied && circuit_->snapshot() == before_failed_open && document_.can_undo();
    checks["successful_copy_open_clears_selection_and_history"] = openDocument(root, "canvas-copy.json") && circuit_->supported() &&
        circuit_->selection().isEmpty() && !current_occurrence_ && !document_.dirty() && !document_.can_undo() && !document_.can_redo();
    checks["copy_reselection_recovers_applied_C"] = click("c") && current_occurrence_->parameters.at("capacitance").value == "0.000000470";
    observe("reopen", "canvas-copy.json");
    checks["reordered_arrays_and_keys_keep_shape_and_ID_click"] = openDocument(root, "reordered.json") && circuit_->supported() && click("c");
    observe("reordered", "reordered.json");
    checks["equal_HTML_labels_are_plain_and_not_identity"] = openDocument(root, "labels.json") && circuit_->supported() && click("c") &&
        current_occurrence_->name == "<b>same Ω label</b>" && circuit_selection_->text().contains("<b>same Ω label</b>") &&
        circuit_selection_->textFormat() == Qt::PlainText;
    observe("labels", "labels.json");
    checks["own_notation_is_independent_of_project_symbol_maps"] = openDocument(root, "swapped.json") && circuit_->supported() && click("c") &&
        !artwork_->capture() && !occurrence_capture_;
    observe("swapped", "swapped.json");
    for(const auto& [stage, input, key] : std::array<std::array<QString, 3>, 5>{{
        {"missing_peer", "missing-peer.json", "missing_endpoint_refuses_false_fixed_wires"},
        {"changed_net", "changed-net.json", "changed_net_ID_refuses_known_layout"},
        {"extra_pin", "extra-pin.json", "extra_pin_refuses_incomplete_fixed_layout"},
        {"wrong_dimension", "wrong-dimension.json", "wrong_dimension_refuses_R_C_notation"},
        {"default_binding", "default-binding.json", "nonforwarded_component_refuses_containing_edit"}}}) {
        checks[key] = openDocument(root, input) && !circuit_->supported() && circuit_->selection().isEmpty() && !click("c") &&
            !circuit_edit_->isEnabled() && !document_.dirty();
        observe(stage, input);
    }
    checks["final_copy_restores_supported_current_shape"] = openDocument(root, "canvas-copy.json") && click("c") &&
        circuit_->supported() && !document_.dirty() && !artwork_->capture() && !occurrence_capture_;
    observe("final", "canvas-copy.json");
    showAnalyzer();
    checks["independent_analyzer_and_adjustable_existing_docks"] = isWindow() && analyzer_.isWindow() && analyzer_.isVisible() &&
        components_->features().testFlag(QDockWidget::DockWidgetClosable) && properties_->features().testFlag(QDockWidget::DockWidgetMovable);
    settle();
    checks["final_canvas_image_and_compact_properties"] = centralWidget() == circuit_panel_ && properties_->widget() == circuit_properties_ &&
        circuit_->width() >= 360 && circuit_->height() >= 280 && circuit_edit_->isVisible() && grab().save(report + ".final.png");
    bool passed = checks.size() == 33;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "One closed declared RC shape, disposable own notation, full-ID scripted click inspection and explicit containing-RC edit; no external artwork, placement/wiring/flattening/ground/source/simulation or human recovery/accessibility/DPI acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runCircuitCapacitanceAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto observe = [&](const QString& stage, const QString& input) {
        const auto literal = selectedCapacitance();
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()},
            {"edit_target", target}, {"draft", capacitance_->text()}, {"unit", literal ? text(literal->unit) : QString{}},
            {"active", capacitance_circuit_host_->isVisible() && capacitance_circuit_host_->isEnabled()},
            {"editor_visible", capacitance_->isVisible()}, {"properties_text", circuit_selection_->text()}});
        if(stage == "left") { settle(); grab().save(report + ".left.png"); }
    };
    const auto click = [&](const QString& id) {
        const auto point = circuit_->componentPoint(id);
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(point), QPointF(circuit_->mapToGlobal(point)),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event); settle();
        const auto context = circuit_context_->currentData().toStringList();
        return circuit_->selection() == QStringList{context[0], context[1], id};
    };
    const auto choose_context = [&](const QString& side) {
        circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main", side})); settle();
    };
    const auto active = [this] { return capacitance_->isVisible() && capacitance_->isEnabled() &&
        apply_capacitance_->isVisible() && apply_capacitance_->isEnabled() && centralWidget() == circuit_panel_; };
    checks["empty_document_refuses_compact_edit"] = !beginCircuitCapacitanceEdit() && !applyCircuitCapacitance() &&
        !circuit_capacitance_edit_->isEnabled() && !capacitance_->isVisible();
    checks["inert_open_does_not_activate_or_select_target"] = openDocument(root, "original.json") && circuit_->supported() &&
        selected_instance_.isEmpty() && circuit_capacitance_target_.isEmpty() && !capacitance_->isVisible() && !document_.dirty();
    settle(); observe("original", "original.json");
    const auto original = document_.graph();
    checks["unselected_apply_refused"] = !applyCircuitCapacitance() && document_.graph() == original;
    checks["resistor_is_read_only_for_C"] = click("r") && !circuit_capacitance_edit_->isEnabled() &&
        !beginCircuitCapacitanceEdit() && !applyCircuitCapacitance() && selected_instance_.isEmpty() && document_.graph() == original;
    checks["C_inspection_does_not_activate_edit"] = click("c") && circuit_capacitance_edit_->isEnabled() &&
        circuit_capacitance_target_.isEmpty() && selected_instance_.isEmpty() && !active() && document_.graph() == original;
    observe("selected", "original.json");
    circuit_capacitance_edit_->click(); settle();
    checks["explicit_button_activates_existing_containing_literal"] = active() && selected_circuit_ == "main" && selected_instance_ == "right" &&
        circuit_capacitance_target_ == QStringList{"main", "right"} && capacitance_->text() == "220" &&
        selectedCapacitance()->unit == "nF" && !details_action_->isChecked() && document_.graph() == original;
    checks["target_unit_and_limits_visible_plain_text"] = circuit_capacitance_target_note_->isVisible() &&
        circuit_capacitance_target_note_->text().contains("main/right") && circuit_capacitance_target_note_->text().contains("nF") &&
        circuit_capacitance_target_note_->textFormat() == Qt::PlainText && capacitance_limits_->isVisible();
    checks["active_image"] = grab().save(report + ".active.png");
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto other_drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" && resistance_->text() == "3.5"; };
    checks["draft_does_not_mutate_applied_canvas"] = active() && document_.graph() == original &&
        current_occurrence_->parameters.at("capacitance").value == "0.000000220" && other_drafts() && capacitanceDraftPending();
    observe("draft", "original.json");
    auto* field = capacitance_; auto* button = apply_capacitance_; auto* panel = capacitance_editor_;
    bool same_widgets = true;
    for(int i = 0; i < 5; ++i) {
        showDeclarationDetails(true); settle();
        same_widgets = same_widgets && capacitance_editor_->parentWidget() == capacitance_details_host_ && capacitance_->isVisible() &&
            capacitance_->text() == "470" && other_drafts();
        showDeclarationDetails(false); settle();
        same_widgets = same_widgets && capacitance_editor_->parentWidget() == capacitance_circuit_host_ && active() &&
            capacitance_->text() == "470" && other_drafts();
    }
    checks["repeated_view_transfer_preserves_one_widget_and_four_drafts"] = same_widgets && capacitance_ == field &&
        apply_capacitance_ == button && capacitance_editor_ == panel && document_.graph() == original;
    checks["draft_image"] = grab().save(report + ".draft.png");
    checks["R_inspection_hides_C_and_refuses_stale_apply"] = click("r") && !active() && !applyCircuitCapacitance() &&
        capacitance_->text() == "470" && other_drafts() && document_.graph() == original;
    checks["same_C_return_recovers_existing_draft_without_rebinding"] = click("c") && active() && capacitance_->text() == "470" &&
        selected_instance_ == "right" && other_drafts();
    choose_context("left"); click("c");
    checks["left_default_refuses_edit_and_stale_apply"] = !circuit_capacitance_edit_->isEnabled() && !beginCircuitCapacitanceEdit() &&
        !applyCircuitCapacitance() && !active() && selected_instance_ == "right" && capacitance_->text() == "470" &&
        other_drafts() && document_.graph() == original;
    choose_context("right"); click("c");
    checks["pending_drafts_block_copy_and_history"] = active() && !saveCopy("pending-circuit-C.json") && !undoEdit() && !redoEdit() &&
        document_.graph() == original && other_drafts() && capacitance_->text() == "470";
    capacitance_->setText("0"); apply_capacitance_->click();
    checks["invalid_value_keeps_draft_graph_and_target"] = active() && capacitance_->text() == "0" && document_.graph() == original &&
        !document_.can_undo() && !document_.can_redo() && other_drafts() && selected_instance_ == "right";
    observe("invalid", "original.json");
    capacitance_->setText("invalid"); apply_capacitance_->click();
    checks["malformed_value_is_not_normalized_or_applied"] = capacitance_->text() == "invalid" && document_.graph() == original && other_drafts();
    capacitance_->setText("470"); apply_capacitance_->click(); settle();
    const auto applied = document_.graph();
    checks["compact_apply_changes_C_and_preserves_other_three_drafts"] = active() && applied != original && document_.dirty() &&
        current_occurrence_->parameters.at("capacitance").value == "0.000000470" && capacitance_->text() == "470" &&
        !capacitanceDraftPending() && other_drafts() && selected_instance_ == "right";
    observe("applied", "circuit-capacitance-copy.json");
    checks["other_drafts_still_block_copy_and_history_after_C_apply"] = !saveCopy("pending-other-C.json") && !undoEdit() &&
        document_.graph() == applied && document_.can_undo() && other_drafts();
    name_->setText(text(document_.name())); instance_name_->setText("right"); resistance_->setText("2.2");
    checks["native_undo_refreshes_active_C_and_canvas"] = undoEdit() && active() && capacitance_->text() == "220" &&
        document_.graph() == original && current_occurrence_->parameters.at("capacitance").value == "0.000000220";
    observe("undo", "original.json");
    capacitance_->setText("0"); apply_capacitance_->click();
    checks["invalid_edit_preserves_redo"] = document_.graph() == original && document_.can_redo() && capacitance_->text() == "0";
    capacitance_->setText("220");
    checks["native_redo_refreshes_active_C_and_canvas"] = redoEdit() && active() && capacitance_->text() == "470" &&
        document_.graph() == applied && current_occurrence_->parameters.at("capacitance").value == "0.000000470";
    observe("redo", "circuit-capacitance-copy.json");
    choose_context("left"); click("c");
    checks["left_inspection_keeps_independent_applied_value"] = !active() && current_occurrence_->parameters.at("capacitance").value == "0.000001" &&
        capacitance_->text() == "470" && selected_instance_ == "right" && document_.graph() == applied;
    observe("left", "circuit-capacitance-copy.json");
    choose_context("right"); click("c");
    checks["create_only_copy_keeps_current_association_and_history"] = saveCopy("circuit-capacitance-copy.json") &&
        document_.leaf() == "original.json" && document_.graph() == applied && document_.can_undo() && active();
    checks["occupied_copy_retains_active_editor"] = !saveCopy("circuit-capacitance-copy.json") && document_.graph() == applied && active();
    checks["failed_open_retains_active_editor_and_history"] = !openDocument(root, "invalid.json") && document_.graph() == applied && active() && document_.can_undo();
    checks["successful_open_clears_activation_drafts_and_history"] = openDocument(root, "circuit-capacitance-copy.json") &&
        circuit_capacitance_target_.isEmpty() && !active() && selected_instance_.isEmpty() && capacitance_->text().isEmpty() &&
        !document_.can_undo() && !document_.can_redo() && !document_.dirty();
    checks["reopened_C_requires_explicit_reactivation"] = click("c") && !active() && beginCircuitCapacitanceEdit() && active() && capacitance_->text() == "470";
    observe("reopened", "circuit-capacitance-copy.json");
    for(const auto& [name, value, key] : std::array<std::array<QString, 3>, 3>{{
        {"instance", "Pending right", "different_target_refused_for_instance_name_draft"},
        {"R", "3.5", "different_target_refused_for_R_draft"},
        {"C", "470", "different_target_refused_for_C_draft"}}}) {
        choose_context("right");
        bool ok = openDocument(root, "left-literal.json") && click("c") && beginCircuitCapacitanceEdit();
        auto* draft = name == "instance" ? instance_name_ : name == "R" ? resistance_ : capacitance_;
        draft->setText(value);
        const auto before = document_.graph();
        choose_context("left"); click("c");
        checks[key] = ok && circuit_capacitance_edit_->isEnabled() && !beginCircuitCapacitanceEdit() && !applyCircuitCapacitance() &&
            !active() && draft->text() == value && selected_instance_ == "right" && document_.graph() == before;
    }
    capacitance_->setText("220"); name_->setText("Pending project");
    checks["clean_instance_switch_preserves_project_draft"] = beginCircuitCapacitanceEdit() && active() && selected_instance_ == "left" &&
        capacitance_->text() == "1000" && name_->text() == "Pending project" && !document_.dirty();
    choose_context("right"); click("c");
    checks["explicit_target_replacement_requires_activation"] = !active() && !applyCircuitCapacitance() && selected_instance_ == "left" &&
        beginCircuitCapacitanceEdit() && active() && selected_instance_ == "right" && capacitance_->text() == "220" && name_->text() == "Pending project";
    for(const auto& [stage, input, key] : std::array<std::array<QString, 3>, 2>{{
        {"reordered", "reordered.json", "reordered_IDs_keep_explicit_target"},
        {"labels", "labels.json", "HTML_labels_are_plain_and_not_edit_identity"}}}) {
        checks[key] = openDocument(root, input) && click("c") && beginCircuitCapacitanceEdit() && active() &&
            selected_circuit_ == "main" && selected_instance_ == "right" && capacitance_->text() == "220" &&
            circuit_capacitance_target_note_->textFormat() == Qt::PlainText;
        observe(stage, input);
    }
    checks["missing_parent_literal_refuses_creation_or_activation"] = openDocument(root, "missing-parent.json") && circuit_->supported() && click("c") &&
        !circuit_capacitance_edit_->isEnabled() && !beginCircuitCapacitanceEdit() && !applyCircuitCapacitance() &&
        selected_instance_.isEmpty() && !document_.dirty() && !active();
    observe("missing_parent", "missing-parent.json");
    checks["unsupported_binding_refuses_stale_editor"] = openDocument(root, "default-binding.json") && !circuit_->supported() &&
        !circuit_capacitance_edit_->isEnabled() && !beginCircuitCapacitanceEdit() && !applyCircuitCapacitance() &&
        circuit_->selection().isEmpty() && selected_instance_.isEmpty() && !active() && !document_.dirty();
    observe("default_binding", "default-binding.json");
    checks["wrong_dimension_refuses_compact_edit"] = openDocument(root, "wrong-dimension.json") && !circuit_->supported() &&
        !beginCircuitCapacitanceEdit() && !applyCircuitCapacitance() && !active() && !document_.dirty();
    choose_context("right");
    checks["final_copy_reactivates_shared_C_editor"] = openDocument(root, "circuit-capacitance-copy.json") && click("c") &&
        beginCircuitCapacitanceEdit() && active() && capacitance_->text() == "470" && !document_.dirty() &&
        !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    observe("final", "circuit-capacitance-copy.json");
    showAnalyzer(); settle();
    checks["independent_windows_and_adjustable_panels_preserved"] = analyzer_.isWindow() && analyzer_.isVisible() && isWindow() &&
        components_->features().testFlag(QDockWidget::DockWidgetClosable) && properties_->features().testFlag(QDockWidget::DockWidgetMovable);
    checks["final_active_C_image"] = active() && grab().save(report + ".final.png");
    QJsonArray captions;
    const std::vector<std::pair<QString, QString>> caption_inputs{
        {"1", "ohm"}, {"999", "ohm"}, {"1000", "ohm"}, {"2200.000", "ohm"},
        {"999999.999999999999999999999999", "ohm"}, {"1000000", "ohm"}, {"999999999", "ohm"},
        {"1000000000", "ohm"}, {"0.1", "ohm"},
        {"0.000000001", "F"}, {"0.000000999", "F"}, {"0.000001", "F"},
        {"0.000999999", "F"}, {"0.001", "F"}, {"0.999", "F"}, {"1", "F"},
        {"0.000000000999", "F"}, {"0.000000220", "F"}, {"0.0000002200000000000000001", "F"},
        {"0", "F"}, {"-0.000001", "F"}, {"1e-6", "F"}, {"001", "ohm"},
        {".", "F"}, {"12x", "ohm"}, {"1", "s"}, {"0,000001", "F"},
        {"2200." + QString(80, '0'), "ohm"}, {QString(129, '1'), "ohm"}};
    for(const auto& [value, unit] : caption_inputs) {
        const simnodus::EffectiveParameter parameter{utf8(value), utf8(unit), "test-only", 0};
        captions.append(QJsonObject{{"value", value}, {"unit", unit}, {"caption", RcCanvas::parameterCaption(parameter)}});
    }
    bool passed = !checks.isEmpty();
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"caption_cases", captions},
        {"scope", "One shared C field with exact fixed RC captions and explicit containing RC activation; existing native operations, no generic quantity editor or human usability/recovery acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runCircuitResistanceAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto observe = [&](const QString& stage, const QString& input) {
        const auto literal = selectedResistance();
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()},
            {"edit_target", target}, {"draft", resistance_->text()}, {"unit", literal ? text(literal->unit) : QString{}},
            {"active", resistance_circuit_host_->isVisible() && resistance_circuit_host_->isEnabled()},
            {"editor_visible", resistance_->isVisible()}, {"properties_text", circuit_selection_->text()}});
    };
    const auto click = [&](const QString& id) {
        const auto point = circuit_->componentPoint(id);
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(point), QPointF(circuit_->mapToGlobal(point)),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event); settle();
        const auto context = circuit_context_->currentData().toStringList();
        return circuit_->selection() == QStringList{context[0], context[1], id};
    };
    const auto choose_context = [&](const QString& side) {
        circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main", side})); settle();
    };
    const auto active = [this] { return resistance_->isVisible() && resistance_->isEnabled() &&
        apply_resistance_->isVisible() && apply_resistance_->isEnabled() && centralWidget() == circuit_panel_; };
    checks["empty_document_refuses_compact_edit"] = !beginCircuitResistanceEdit() && !applyCircuitResistance() &&
        !circuit_resistance_edit_->isEnabled() && !resistance_->isVisible();
    checks["inert_open_does_not_activate_or_select_target"] = openDocument(root, "original.json") && circuit_->supported() &&
        selected_instance_.isEmpty() && circuit_resistance_target_.isEmpty() && !resistance_->isVisible() && !document_.dirty();
    settle(); observe("original", "original.json");
    const auto original = document_.graph();
    checks["unselected_apply_refused"] = !applyCircuitResistance() && document_.graph() == original;
    checks["capacitor_is_read_only_for_R"] = click("c") && !circuit_resistance_edit_->isEnabled() &&
        !beginCircuitResistanceEdit() && !applyCircuitResistance() && selected_instance_.isEmpty() && document_.graph() == original;
    checks["R_inspection_does_not_activate_edit"] = click("r") && circuit_resistance_edit_->isEnabled() &&
        circuit_resistance_target_.isEmpty() && selected_instance_.isEmpty() && !active() && document_.graph() == original;
    observe("selected", "original.json");
    circuit_resistance_edit_->click(); settle();
    checks["explicit_button_activates_existing_containing_literal"] = active() && selected_circuit_ == "main" && selected_instance_ == "right" &&
        circuit_resistance_target_ == QStringList{"main", "right"} && resistance_->text() == "2.2" &&
        selectedResistance()->unit == "kohm" && !details_action_->isChecked() && document_.graph() == original;
    checks["target_unit_and_limits_visible_plain_text"] = circuit_resistance_target_note_->isVisible() &&
        circuit_resistance_target_note_->text().contains("main/right") && circuit_resistance_target_note_->text().contains("kohm") &&
        circuit_resistance_target_note_->textFormat() == Qt::PlainText && resistance_limits_->isVisible();
    checks["active_image"] = grab().save(report + ".active.png");
    name_->setText("Pending project"); instance_name_->setText("Pending right"); capacitance_->setText("470"); resistance_->setText("3.5");
    const auto other_drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" && capacitance_->text() == "470"; };
    checks["draft_does_not_mutate_applied_canvas"] = active() && document_.graph() == original &&
        current_occurrence_->parameters.at("resistance").value == "2200" && other_drafts() && resistanceDraftPending();
    observe("draft", "original.json");
    auto* field = resistance_; auto* button = apply_resistance_; auto* panel = resistance_editor_;
    bool same_widgets = true;
    for(int i = 0; i < 5; ++i) {
        showDeclarationDetails(true); settle();
        same_widgets = same_widgets && resistance_editor_->parentWidget() == resistance_details_host_ && resistance_->isVisible() &&
            resistance_->text() == "3.5" && other_drafts();
        showDeclarationDetails(false); settle();
        same_widgets = same_widgets && resistance_editor_->parentWidget() == resistance_circuit_host_ && active() &&
            resistance_->text() == "3.5" && other_drafts();
    }
    checks["repeated_view_transfer_preserves_one_widget_and_four_drafts"] = same_widgets && resistance_ == field &&
        apply_resistance_ == button && resistance_editor_ == panel && document_.graph() == original;
    checks["draft_image"] = grab().save(report + ".draft.png");
    checks["C_inspection_hides_R_and_refuses_stale_apply"] = click("c") && !active() && !applyCircuitResistance() &&
        resistance_->text() == "3.5" && other_drafts() && document_.graph() == original;
    checks["same_R_return_recovers_existing_draft_without_rebinding"] = click("r") && active() && resistance_->text() == "3.5" &&
        selected_instance_ == "right" && other_drafts();
    click("c");
    checks["R_C_activation_shares_target_but_keeps_separate_widgets_drafts"] = beginCircuitCapacitanceEdit() &&
        capacitance_->isVisible() && !active() && circuit_capacitance_target_ == circuit_resistance_target_ &&
        capacitance_->text() == "470" && resistance_->text() == "3.5" && other_drafts() &&
        capacitance_editor_ != resistance_editor_ && document_.graph() == original && !applyCircuitResistance();
    click("r");
    checks["opposite_C_Apply_refused_while_R_selected"] = active() && !capacitance_->isVisible() &&
        !applyCircuitCapacitance() && resistance_->text() == "3.5" && other_drafts() && document_.graph() == original;
    choose_context("left"); click("r");
    checks["left_existing_requires_activation_and_retains_right_draft"] = circuit_resistance_edit_->isEnabled() && !beginCircuitResistanceEdit() &&
        !applyCircuitResistance() && !active() && selected_instance_ == "right" && resistance_->text() == "3.5" &&
        other_drafts() && document_.graph() == original;
    choose_context("right"); click("r");
    checks["pending_drafts_block_copy_and_history"] = active() && !saveCopy("pending-circuit-R.json") && !undoEdit() && !redoEdit() &&
        document_.graph() == original && other_drafts() && resistance_->text() == "3.5";
    resistance_->setText("0"); apply_resistance_->click();
    checks["invalid_value_keeps_draft_graph_and_target"] = active() && resistance_->text() == "0" && document_.graph() == original &&
        !document_.can_undo() && !document_.can_redo() && other_drafts() && selected_instance_ == "right";
    observe("invalid", "original.json");
    resistance_->setText("invalid"); apply_resistance_->click();
    checks["malformed_value_is_not_normalized_or_applied"] = resistance_->text() == "invalid" && document_.graph() == original && other_drafts();
    resistance_->setText("3.5"); apply_resistance_->click(); settle();
    const auto applied = document_.graph();
    checks["compact_apply_changes_R_and_preserves_other_three_drafts"] = active() && applied != original && document_.dirty() &&
        current_occurrence_->parameters.at("resistance").value == "3500" && resistance_->text() == "3.5" &&
        !resistanceDraftPending() && other_drafts() && selected_instance_ == "right";
    observe("applied", "circuit-resistance-copy.json");
    checks["other_drafts_still_block_copy_and_history_after_R_apply"] = !saveCopy("pending-other-R.json") && !undoEdit() &&
        document_.graph() == applied && document_.can_undo() && other_drafts();
    name_->setText(text(document_.name())); instance_name_->setText("right"); capacitance_->setText("220");
    checks["native_undo_refreshes_active_R_and_canvas"] = undoEdit() && active() && resistance_->text() == "2.2" &&
        document_.graph() == original && current_occurrence_->parameters.at("resistance").value == "2200";
    observe("undo", "original.json");
    resistance_->setText("0"); apply_resistance_->click();
    checks["invalid_edit_preserves_redo"] = document_.graph() == original && document_.can_redo() && resistance_->text() == "0";
    resistance_->setText("2.2");
    checks["native_redo_refreshes_active_R_and_canvas"] = redoEdit() && active() && resistance_->text() == "3.5" &&
        document_.graph() == applied && current_occurrence_->parameters.at("resistance").value == "3500";
    observe("redo", "circuit-resistance-copy.json");
    choose_context("left"); click("r");
    checks["left_inspection_keeps_independent_applied_value"] = !active() && current_occurrence_->parameters.at("resistance").value == "1000" &&
        resistance_->text() == "3.5" && selected_instance_ == "right" && document_.graph() == applied;
    observe("left", "circuit-resistance-copy.json");
    choose_context("right"); click("r");
    checks["create_only_copy_keeps_current_association_and_history"] = saveCopy("circuit-resistance-copy.json") &&
        document_.leaf() == "original.json" && document_.graph() == applied && document_.can_undo() && active();
    checks["occupied_copy_retains_active_editor"] = !saveCopy("circuit-resistance-copy.json") && document_.graph() == applied && active();
    checks["failed_open_retains_active_editor_and_history"] = !openDocument(root, "invalid.json") && document_.graph() == applied && active() && document_.can_undo();
    checks["successful_open_clears_activation_drafts_and_history"] = openDocument(root, "circuit-resistance-copy.json") &&
        circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() && !active() &&
        !capacitance_->isVisible() && capacitance_->text().isEmpty() && selected_instance_.isEmpty() && resistance_->text().isEmpty() &&
        !document_.can_undo() && !document_.can_redo() && !document_.dirty();
    checks["reopened_R_requires_explicit_reactivation"] = click("r") && !active() && beginCircuitResistanceEdit() && active() && resistance_->text() == "3.5";
    observe("reopened", "circuit-resistance-copy.json");
    for(const auto& [name, value, key] : std::array<std::array<QString, 3>, 3>{{
        {"instance", "Pending right", "different_target_refused_for_instance_name_draft"},
        {"C", "470", "different_target_refused_for_C_draft"},
        {"R", "3.5", "different_target_refused_for_R_draft"}}}) {
        choose_context("right");
        bool ok = openDocument(root, "original.json") && click("r") && beginCircuitResistanceEdit();
        auto* draft = name == "instance" ? instance_name_ : name == "C" ? capacitance_ : resistance_;
        draft->setText(value);
        const auto before = document_.graph();
        choose_context("left"); click("r");
        checks[key] = ok && circuit_resistance_edit_->isEnabled() && !beginCircuitResistanceEdit() && !applyCircuitResistance() &&
            !active() && draft->text() == value && selected_instance_ == "right" && document_.graph() == before;
    }
    resistance_->setText("2.2"); name_->setText("Pending project");
    checks["clean_instance_switch_preserves_project_draft"] = beginCircuitResistanceEdit() && active() && selected_instance_ == "left" &&
        resistance_->text() == "1" && name_->text() == "Pending project" && !document_.dirty();
    choose_context("right"); click("r");
    checks["explicit_target_replacement_requires_activation"] = !active() && !applyCircuitResistance() && selected_instance_ == "left" &&
        beginCircuitResistanceEdit() && active() && selected_instance_ == "right" && resistance_->text() == "2.2" && name_->text() == "Pending project";
    for(const auto& [stage, input, key] : std::array<std::array<QString, 3>, 2>{{
        {"reordered", "reordered.json", "reordered_IDs_keep_explicit_target"},
        {"labels", "labels.json", "HTML_labels_are_plain_and_not_edit_identity"}}}) {
        checks[key] = openDocument(root, input) && click("r") && beginCircuitResistanceEdit() && active() &&
            selected_circuit_ == "main" && selected_instance_ == "right" && resistance_->text() == "2.2" &&
            circuit_resistance_target_note_->textFormat() == Qt::PlainText;
        observe(stage, input);
    }
    checks["missing_parent_literal_refuses_creation_or_activation"] = openDocument(root, "missing-parent.json") && circuit_->supported() && click("r") &&
        !circuit_resistance_edit_->isEnabled() && !beginCircuitResistanceEdit() && !applyCircuitResistance() &&
        selected_instance_.isEmpty() && !document_.dirty() && !active();
    observe("missing_parent", "missing-parent.json");
    checks["unsupported_binding_refuses_stale_editor"] = openDocument(root, "default-binding.json") && !circuit_->supported() &&
        !circuit_resistance_edit_->isEnabled() && !beginCircuitResistanceEdit() && !applyCircuitResistance() &&
        circuit_->selection().isEmpty() && selected_instance_.isEmpty() && !active() && !document_.dirty();
    observe("default_binding", "default-binding.json");
    checks["wrong_dimension_refuses_compact_edit"] = openDocument(root, "wrong-dimension.json") && !circuit_->supported() &&
        !beginCircuitResistanceEdit() && !applyCircuitResistance() && !active() && !document_.dirty();
    choose_context("right");
    checks["final_copy_reactivates_shared_R_editor"] = openDocument(root, "circuit-resistance-copy.json") && click("r") &&
        beginCircuitResistanceEdit() && active() && resistance_->text() == "3.5" && !document_.dirty() &&
        !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    observe("final", "circuit-resistance-copy.json");
    showAnalyzer(); settle();
    checks["independent_windows_and_adjustable_panels_preserved"] = analyzer_.isWindow() && analyzer_.isVisible() && isWindow() &&
        components_->features().testFlag(QDockWidget::DockWidgetClosable) && properties_->features().testFlag(QDockWidget::DockWidgetMovable);
    checks["final_active_R_image"] = active() && grab().save(report + ".final.png");
    bool passed = checks.size() == 43;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "One shared R field with explicit containing RC activation and independent C draft/activation; existing native operations, no generic quantity editor or human usability/recovery acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runZoomAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto noteFits = [&](QLabel* label) {
        settle();
        return label->isVisible() && label->height() >= label->heightForWidth(label->width());
    };
    const auto activeR = [this] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [this] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    const auto click = [&](const QString& id) {
        // Separate literal centers from the production componentPoint helper.
        const QPointF model = id == "r" ? QPointF(255, 190) : QPointF(445, 280);
        const auto scale = std::min((circuit_->width() - 24.0) / 700, (circuit_->height() - 24.0) / 420) * circuit_->zoomPercent() / 100.0;
        const auto point = QPointF(circuit_->width() / 2.0 + (model.x() - 350) * scale,
            circuit_->height() / 2.0 + (model.y() - 210) * scale).toPoint();
        QMouseEvent event(QEvent::MouseButtonPress, QPointF(point), QPointF(circuit_->mapToGlobal(point)),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event); settle();
        const auto context = circuit_context_->currentData().toStringList();
        return circuit_->selection() == QStringList{context[0], context[1], id};
    };
    const auto choose = [&](const QString& side) {
        circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main", side})); settle();
    };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()},
            {"edit_target", target}, {"r_draft", resistance_->text()}, {"c_draft", capacitance_->text()},
            {"r_active", activeR()}, {"c_active", activeC()}, {"properties_text", circuit_selection_->text()}});
    };
    checks["empty_controls_disabled_and_fit_default"] = !circuit_zoom_in_->isEnabled() && !circuit_zoom_out_->isEnabled() &&
        !circuit_fit_->isEnabled() && !circuit_->zoomIn() && !circuit_->zoomOut() && circuit_->zoomPercent() == 100;
    checks["inert_open_fit_without_selection_or_edits"] = openDocument(root, "original.json") && circuit_->supported() &&
        circuit_->zoomPercent() == 100 && circuit_->selection().isEmpty() && selected_instance_.isEmpty() && !document_.dirty() &&
        circuit_zoom_in_->isEnabled() && circuit_zoom_out_->isEnabled() && circuit_fit_->isEnabled();
    settle(); observe("original", "original.json");
    const auto original = document_.graph();
    circuit_zoom_in_->click(); settle();
    checks["zoom_125_preserves_graph_and_selection"] = circuit_->zoomPercent() == 125 && document_.graph() == original &&
        circuit_->selection().isEmpty() && selected_instance_.isEmpty() && circuit_zoom_note_->text() == "Zoom: 125%";
    observe("zoom125", "original.json");
    circuit_zoom_in_->click(); settle();
    checks["zoom_150_upper_limit_and_R_hit"] = circuit_->zoomPercent() == 150 && !circuit_zoom_in_->isEnabled() &&
        !circuit_->zoomIn() && click("r") && beginCircuitResistanceEdit() && activeR() && document_.graph() == original &&
        noteFits(circuit_resistance_target_note_);
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    observe("zoom150_R", "original.json");
    checks["C_hit_at_150_preserves_R_draft"] = click("c") && beginCircuitCapacitanceEdit() && activeC() && !activeR() && drafts() &&
        noteFits(circuit_capacitance_target_note_);
    observe("zoom150_C", "original.json");
    checks["zoom_retains_four_drafts_and_independent_editors"] = drafts() && document_.graph() == original &&
        circuit_capacitance_target_ == circuit_resistance_target_ && capacitance_editor_ != resistance_editor_ &&
        grab().save(report + ".zoom150.png");
    circuit_fit_->click(); showDeclarationDetails(true); settle(); showDeclarationDetails(false); settle();
    checks["fit_and_details_transfer_keep_active_C_and_drafts"] = circuit_->zoomPercent() == 100 && activeC() && drafts() &&
        capacitance_editor_->parentWidget() == capacitance_circuit_host_ && resistance_editor_->parentWidget() == resistance_circuit_host_;
    observe("fit_C", "original.json");
    circuit_zoom_out_->click(); circuit_zoom_out_->click(); settle();
    checks["zoom_50_lower_limit_and_independent_R_hit"] = circuit_->zoomPercent() == 50 && !circuit_zoom_out_->isEnabled() &&
        !circuit_->zoomOut() && click("r") && activeR() && drafts() && document_.graph() == original;
    observe("zoom50_R", "original.json");
    resize(1200, 850); settle();
    checks["resize_at_50_retains_scale_and_C_hit"] = circuit_->zoomPercent() == 50 && click("c") && activeC() && drafts();
    observe("resize50_C", "original.json");
    choose("left"); click("r");
    checks["context_switch_retains_zoom_and_refuses_stale_apply"] = circuit_->zoomPercent() == 50 && !activeR() &&
        !activeC() && !applyCircuitResistance() && !applyCircuitCapacitance() && !beginCircuitResistanceEdit() && drafts() &&
        selected_instance_ == "right" && document_.graph() == original;
    observe("left50_R", "original.json");
    choose("right");
    checks["return_right_recovers_R_draft"] = click("r") && activeR() && drafts() && circuit_->zoomPercent() == 50;
    name_->setText(text(document_.name())); instance_name_->setText("right");
    circuit_fit_->click(); circuit_zoom_in_->click(); click("c"); apply_capacitance_->click(); settle();
    const auto C_applied = document_.graph();
    checks["C_apply_at_125_preserves_R_draft_and_zoom"] = circuit_->zoomPercent() == 125 && activeC() && C_applied != original &&
        capacitance_->text() == "470" && resistance_->text() == "3.5" && current_occurrence_->parameters.at("capacitance").value == "0.000000470";
    observe("C_applied", "zoom-C-copy.json");
    checks["pending_R_refuses_history_and_copy"] = !saveCopy("pending-zoom-copy.json") && !undoEdit() &&
        circuit_->zoomPercent() == 125 && document_.graph() == C_applied && resistance_->text() == "3.5";
    resistance_->setText("2.2");
    const auto C_saved = saveCopy("zoom-C-copy.json");
    click("r"); resistance_->setText("3.5"); apply_resistance_->click(); settle();
    const auto both_applied = document_.graph();
    checks["R_apply_at_125_preserves_C_and_zoom"] = C_saved && circuit_->zoomPercent() == 125 && activeR() &&
        both_applied != C_applied && resistance_->text() == "3.5" && capacitance_->text() == "470" &&
        current_occurrence_->parameters.at("resistance").value == "3500";
    observe("R_applied", "zoom-RC-copy.json");
    circuit_zoom_in_->click();
    checks["native_undo_at_150_retains_view_and_C"] = undoEdit() && circuit_->zoomPercent() == 150 && activeR() &&
        document_.graph() == C_applied && resistance_->text() == "2.2" && capacitance_->text() == "470";
    observe("undo150_R", "zoom-C-copy.json");
    checks["native_redo_at_150_retains_view_and_R"] = redoEdit() && circuit_->zoomPercent() == 150 && activeR() &&
        document_.graph() == both_applied && resistance_->text() == "3.5" && capacitance_->text() == "470";
    observe("redo150_R", "zoom-RC-copy.json");
    checks["create_only_copy_keeps_zoom_and_association"] = saveCopy("zoom-RC-copy.json") && circuit_->zoomPercent() == 150 &&
        document_.leaf() == "original.json" && document_.graph() == both_applied && document_.can_undo();
    checks["occupied_copy_refusal_retains_zoom"] = !saveCopy("zoom-RC-copy.json") && circuit_->zoomPercent() == 150 && activeR();
    checks["failed_open_retains_zoom_and_drafts"] = !openDocument(root, "invalid.json") && circuit_->zoomPercent() == 150 &&
        activeR() && resistance_->text() == "3.5" && capacitance_->text() == "470" && document_.graph() == both_applied;
    checks["copy_open_resets_fit_selection_activation_history"] = openDocument(root, "zoom-RC-copy.json") &&
        circuit_->zoomPercent() == 100 && circuit_->selection().isEmpty() && circuit_resistance_target_.isEmpty() &&
        circuit_capacitance_target_.isEmpty() && selected_instance_.isEmpty() && resistance_->text().isEmpty() &&
        capacitance_->text().isEmpty() && !document_.can_undo() && !document_.can_redo() && !document_.dirty();
    settle(); observe("reopened", "zoom-RC-copy.json");
    circuit_zoom_in_->click(); circuit_zoom_in_->click();
    checks["unsupported_shape_clears_zoom_and_disables_controls"] = openDocument(root, "changed-net.json") && !circuit_->supported() &&
        circuit_->zoomPercent() == 100 && !circuit_zoom_in_->isEnabled() && !circuit_zoom_out_->isEnabled() && !circuit_fit_->isEnabled() &&
        !circuit_->zoomIn() && !circuit_->zoomOut() && circuit_->selection().isEmpty();
    settle(); observe("unsupported", "changed-net.json");
    openDocument(root, "zoom-RC-copy.json"); choose("left"); circuit_zoom_in_->click();
    checks["left_context_can_zoom_inspect_and_activate_independent_R"] = click("r") && beginCircuitResistanceEdit() && activeR() &&
        circuit_->zoomPercent() == 125 && selected_instance_ == "left" && resistance_->text() == "1" && !document_.dirty();
    observe("left125_R", "zoom-RC-copy.json");
    checks["zoom_actions_do_not_access_resources"] = !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    choose("right"); click("r"); beginCircuitResistanceEdit(); circuit_fit_->click(); settle();
    checks["fit_restores_full_default_view_without_edit"] = circuit_->zoomPercent() == 100 && activeR() && resistance_->text() == "3.5" &&
        capacitance_->text() == "470" && !document_.dirty() && grab().save(report + ".fit.png");
    observe("final_fit_R", "zoom-RC-copy.json");
    showAnalyzer(); settle();
    checks["independent_windows_panels_and_final_image"] = analyzer_.isWindow() && analyzer_.isVisible() && isWindow() &&
        components_->features().testFlag(QDockWidget::DockWidgetClosable) && properties_->features().testFlag(QDockWidget::DockWidgetMovable) &&
        grab().save(report + ".final.png");
    bool passed = checks.size() == 25;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "Bounded centered fit/zoom on the fixed RC view and existing native R/C operations; no stored geometry, pan, wheel, keyboard, DPI/session or human recovery acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runPanAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto activeR = [this] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [this] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    QPointF expected_pan, mouse_position;
    const auto scale = [this] {
        return std::min((circuit_->width() - 24.0) / 700, (circuit_->height() - 24.0) / 420) * circuit_->zoomPercent() / 100.0;
    };
    const auto project = [&](const QPointF& point) {
        return QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0) +
            (point - QPointF(350, 210) + expected_pan) * scale();
    };
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, const QPointF& point) {
        QMouseEvent event(type, point, QPointF(circuit_->mapToGlobal(point.toPoint())), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event); settle();
    };
    const auto press = [&] {
        settle();
        mouse_position = QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0);
        send(QEvent::MouseButtonPress, Qt::MiddleButton, Qt::MiddleButton, mouse_position);
    };
    const auto move = [&](const QPointF& delta, Qt::MouseButtons buttons, bool changes_pan) {
        // Actor coordinates come from literal expected offsets, never componentPoint or the reported view.
        mouse_position += delta * scale();
        if(changes_pan) expected_pan = QPointF(std::clamp(expected_pan.x() + delta.x(), -140.0, 140.0),
            std::clamp(expected_pan.y() + delta.y(), -84.0, 84.0));
        send(QEvent::MouseMove, Qt::NoButton, buttons, mouse_position);
    };
    const auto release = [&] { send(QEvent::MouseButtonRelease, Qt::MiddleButton, Qt::NoButton, mouse_position); };
    const auto drag = [&](const QPointF& delta) { press(); move(delta, Qt::MiddleButton, true); release(); };
    const auto isPan = [&](const QPointF& wanted, bool dragging = false) {
        const auto view = circuit_->snapshot()["view"].toObject();
        const auto pan = view["pan"].toArray();
        return std::abs(pan[0].toDouble() - wanted.x()) < 0.000001 && std::abs(pan[1].toDouble() - wanted.y()) < 0.000001 &&
            view["panning"].toBool() == dragging;
    };
    const auto click = [&](const QString& id) {
        send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, project(id == "r" ? QPointF(255,190) : QPointF(445,280)));
        const auto context = circuit_context_->currentData().toStringList();
        return circuit_->selection() == QStringList{context[0], context[1], id};
    };
    const auto choose = [&](const QString& side) {
        circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main", side})); settle();
    };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()},
            {"edit_target", target}, {"r_draft", resistance_->text()}, {"c_draft", capacitance_->text()},
            {"r_active", activeR()}, {"c_active", activeC()}, {"properties_text", circuit_selection_->text()}});
    };
    press(); move({20, 15}, Qt::MiddleButton, false); release();
    checks["empty_middle_refuses_and_cursor_unchanged"] = isPan({0,0}) && circuit_->cursor().shape() != Qt::ClosedHandCursor;
    checks["inert_open_centered"] = openDocument(root, "original.json") && circuit_->supported() && isPan({0,0}) &&
        circuit_->zoomPercent() == 100 && circuit_->selection().isEmpty() && selected_instance_.isEmpty() && !document_.dirty();
    settle(); observe("original", "original.json");
    const auto original = document_.graph();
    circuit_zoom_in_->click(); circuit_zoom_in_->click(); click("r"); beginCircuitResistanceEdit();
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    checks["zoom150_R_activation_and_drafts"] = circuit_->zoomPercent() == 150 && activeR() && drafts() && isPan({0,0}) && document_.graph() == original;
    observe("start_R", "original.json");
    press();
    checks["middle_press_preserves_selection"] = isPan({0,0}, true) && circuit_->selection() == QStringList{"main","right","r"} &&
        circuit_->cursor().shape() == Qt::ClosedHandCursor && drafts();
    observe("press_R", "original.json");
    move({40,-25}, Qt::MiddleButton, true);
    checks["middle_move_updates_only_view"] = isPan({40,-25}, true) && activeR() && drafts() && document_.graph() == original;
    observe("drag_R", "original.json");
    send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::MiddleButton | Qt::LeftButton, project({445,280}));
    checks["left_press_during_middle_drag_does_not_select_C"] = circuit_->selection() == QStringList{"main","right","r"} &&
        isPan({40,-25}, true) && activeR();
    release();
    checks["release_stops_and_cursor_restores"] = isPan({40,-25}) && circuit_->cursor().shape() != Qt::ClosedHandCursor && drafts();
    observe("released_R", "original.json");
    move({20,15}, Qt::MiddleButton, false);
    send(QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton, project({445,280}));
    checks["moves_without_drag_and_right_press_inert"] = isPan({40,-25}) && activeR() && drafts() && document_.graph() == original;
    press(); move({1000,1000}, Qt::MiddleButton, true);
    const auto clamped = isPan({140,84}, true);
    move({-1,-1}, Qt::MiddleButton, true); const auto reverse = isPan({139,83}, true);
    move({1,1}, Qt::MiddleButton, true); release();
    checks["upper_pan_bounds_and_C_hit"] = clamped && reverse && isPan({140,84}) && click("c") &&
        beginCircuitCapacitanceEdit() && activeC() && !activeR() && drafts();
    observe("max_C", "original.json");
    drag({-2000,-2000});
    checks["lower_pan_bounds_and_R_hit"] = isPan({-140,-84}) && click("r") && activeR() && !activeC() && drafts();
    observe("min_R", "original.json");
    press(); move({10,10}, Qt::MiddleButton, true); move({20,20}, Qt::NoButton, false);
    checks["lost_middle_button_cancels_view_only"] = isPan({-130,-74}) && circuit_->cursor().shape() != Qt::ClosedHandCursor && drafts();
    observe("lost_button_R", "original.json");
    press(); move({10,10}, Qt::MiddleButton, true);
    QEvent ungrab(QEvent::UngrabMouse); QApplication::sendEvent(circuit_, &ungrab); settle();
    const auto ungrabbed = isPan({-120,-64}) && circuit_->cursor().shape() != Qt::ClosedHandCursor;
    press(); QEvent deactivate(QEvent::WindowDeactivate); QApplication::sendEvent(circuit_, &deactivate); settle();
    checks["ungrab_and_deactivate_cancel_view_only"] = ungrabbed && isPan({-120,-64}) &&
        circuit_->cursor().shape() != Qt::ClosedHandCursor && activeR() && drafts();
    observe("ungrab_R", "original.json");
    press(); move({20,14}, Qt::MiddleButton, true); circuit_fit_->click(); expected_pan = {}; settle();
    move({20,15}, Qt::MiddleButton, false);
    checks["fit_cancels_resets_and_preserves_drafts"] = isPan({0,0}) && circuit_->zoomPercent() == 100 && activeR() && drafts();
    observe("fit_R", "original.json");
    press(); move({35,20}, Qt::MiddleButton, true); circuit_zoom_in_->click(); settle(); move({20,15}, Qt::MiddleButton, false);
    checks["zoom_cancels_and_retains_pan_and_drafts"] = isPan({35,20}) && circuit_->zoomPercent() == 125 && activeR() && drafts();
    observe("zoom_R", "original.json");
    press(); resize(1200,850); settle();
    checks["resize_cancels_retains_pan_and_C_hit"] = isPan({35,20}) && circuit_->zoomPercent() == 125 && click("c") && activeC() && drafts();
    observe("resize_C", "original.json");
    press(); showDeclarationDetails(true); settle(); showDeclarationDetails(false); settle();
    checks["details_hide_cancels_retains_pan_fields"] = isPan({35,20}) && activeC() && drafts() &&
        capacitance_editor_->parentWidget() == capacitance_circuit_host_ && resistance_editor_->parentWidget() == resistance_circuit_host_;
    observe("details_C", "original.json");
    choose("left"); click("r");
    checks["context_retains_pan_refuses_stale_apply"] = isPan({35,20}) && !activeR() && !activeC() &&
        !applyCircuitResistance() && !applyCircuitCapacitance() && !beginCircuitResistanceEdit() && drafts() && selected_instance_ == "right";
    observe("left_R", "original.json");
    choose("right"); click("c"); name_->setText(text(document_.name())); instance_name_->setText("right");
    apply_capacitance_->click(); settle(); const auto C_applied = document_.graph();
    checks["C_apply_retains_pan_R_draft"] = C_applied != original && isPan({35,20}) && circuit_->zoomPercent() == 125 &&
        activeC() && resistance_->text() == "3.5" && capacitance_->text() == "470";
    observe("C_applied", "pan-C-copy.json");
    checks["pending_R_refuses_history_copy"] = !saveCopy("pending-pan-copy.json") && !undoEdit() && isPan({35,20}) && document_.graph() == C_applied;
    resistance_->setText("2.2"); const auto C_saved = saveCopy("pan-C-copy.json");
    click("r"); resistance_->setText("3.5"); apply_resistance_->click(); settle(); const auto both_applied = document_.graph();
    checks["R_apply_retains_pan_C"] = C_saved && both_applied != C_applied && isPan({35,20}) &&
        circuit_->zoomPercent() == 125 && activeR() && capacitance_->text() == "470";
    observe("R_applied", "pan-RC-copy.json");
    checks["native_undo_retains_pan_C"] = undoEdit() && isPan({35,20}) && circuit_->zoomPercent() == 125 &&
        activeR() && document_.graph() == C_applied && resistance_->text() == "2.2" && capacitance_->text() == "470";
    observe("undo_R", "pan-C-copy.json");
    checks["native_redo_retains_pan_R"] = redoEdit() && isPan({35,20}) && circuit_->zoomPercent() == 125 &&
        activeR() && document_.graph() == both_applied && resistance_->text() == "3.5" && capacitance_->text() == "470";
    observe("redo_R", "pan-RC-copy.json");
    checks["create_only_and_occupied_copy_keep_pan"] = saveCopy("pan-RC-copy.json") && !saveCopy("pan-RC-copy.json") &&
        isPan({35,20}) && document_.leaf() == "original.json" && document_.graph() == both_applied && document_.can_undo();
    checks["failed_open_keeps_pan_drafts"] = !openDocument(root,"invalid.json") && isPan({35,20}) &&
        activeR() && resistance_->text() == "3.5" && capacitance_->text() == "470" && document_.graph() == both_applied;
    checks["copy_open_resets_pan_zoom_selection_activations_history"] = openDocument(root,"pan-RC-copy.json") &&
        isPan({0,0}) && circuit_->zoomPercent() == 100 && circuit_->selection().isEmpty() && circuit_resistance_target_.isEmpty() &&
        circuit_capacitance_target_.isEmpty() && selected_instance_.isEmpty() && !document_.can_undo() && !document_.can_redo() && !document_.dirty();
    expected_pan = {}; settle(); observe("reopened", "pan-RC-copy.json");
    drag({15,-8}); circuit_zoom_in_->click();
    const auto unsupported = openDocument(root,"changed-net.json"); expected_pan = {}; press(); move({20,15}, Qt::MiddleButton, false); release();
    checks["unsupported_disables_pan_resets"] = unsupported && !circuit_->supported() && isPan({0,0}) &&
        circuit_->zoomPercent() == 100 && !circuit_zoom_in_->isEnabled() && !circuit_fit_->isEnabled() && circuit_->selection().isEmpty();
    observe("unsupported", "changed-net.json");
    openDocument(root,"pan-RC-copy.json"); choose("right"); click("r"); beginCircuitResistanceEdit();
    circuit_zoom_in_->click(); circuit_zoom_in_->click(); drag({90,30}); click("r");
    const auto visible_input = QRectF(circuit_->rect()).contains(project({70,190})) && QRectF(circuit_->rect()).contains(project({70,350}));
    const auto input_image = grab().save(report + ".pan-input.png");
    observe("pan_input_R", "pan-RC-copy.json");
    drag({-180,0}); click("r");
    checks["pan_reaches_original_input_reference_and_output"] = visible_input && input_image && isPan({-90,30}) &&
        QRectF(circuit_->rect()).contains(project({625,190})) && grab().save(report + ".pan-output.png");
    observe("pan_output_R", "pan-RC-copy.json");
    circuit_fit_->click(); expected_pan = {}; settle(); showAnalyzer(); settle();
    checks["final_fit_windows_panels_resources"] = isPan({0,0}) && circuit_->zoomPercent() == 100 && activeR() && !document_.dirty() &&
        analyzer_.isWindow() && analyzer_.isVisible() && isWindow() && !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1 &&
        components_->features().testFlag(QDockWidget::DockWidgetClosable) && properties_->features().testFlag(QDockWidget::DockWidgetMovable) &&
        grab().save(report + ".fit.png");
    observe("final_fit_R", "pan-RC-copy.json");
    bool passed = checks.size() == 28;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "Bounded middle-button pan on the fixed RC view; synthetic cancellation only, no persisted geometry, human mouse/recovery/OS focus, keyboard, DPI/session or packaging acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runWheelAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto activeR = [this] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [this] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    QPointF expected_pan, mouse_position;
    const auto scale = [this] {
        return std::min((circuit_->width() - 24.0) / 700, (circuit_->height() - 24.0) / 420) * circuit_->zoomPercent() / 100.0;
    };
    const auto project = [&](const QPointF& point) {
        return QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0) +
            (point - QPointF(350, 210) + expected_pan) * scale();
    };
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, const QPointF& point) {
        QMouseEvent event(type, point, QPointF(circuit_->mapToGlobal(point.toPoint())), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event); settle();
    };
    const auto wheel = [&](QPoint angle, QPoint pixel = {}, Qt::ScrollPhase phase = Qt::NoScrollPhase,
                           bool inverted = false, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                           Qt::MouseButtons buttons = Qt::NoButton) {
        settle();
        const QPointF point(circuit_->width() / 2.0, circuit_->height() / 2.0);
        QWheelEvent event(point, QPointF(circuit_->mapToGlobal(point.toPoint())), pixel, angle,
            buttons, modifiers, phase, inverted, Qt::MouseEventSynthesizedByApplication);
        event.ignore(); QApplication::sendEvent(circuit_, &event); settle();
        return event.isAccepted();
    };
    const auto press = [&] {
        settle(); mouse_position = QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0);
        send(QEvent::MouseButtonPress, Qt::MiddleButton, Qt::MiddleButton, mouse_position);
    };
    const auto move = [&](const QPointF& delta, bool changes_pan) {
        mouse_position += delta * scale();
        if(changes_pan) expected_pan += delta;
        send(QEvent::MouseMove, Qt::NoButton, Qt::MiddleButton, mouse_position);
    };
    const auto drag = [&](const QPointF& delta) {
        press(); move(delta, true);
        send(QEvent::MouseButtonRelease, Qt::MiddleButton, Qt::NoButton, mouse_position);
    };
    const auto isPan = [&](const QPointF& wanted, bool dragging = false) {
        const auto view = circuit_->snapshot()["view"].toObject();
        const auto pan = view["pan"].toArray();
        return std::abs(pan[0].toDouble() - wanted.x()) < 0.000001 && std::abs(pan[1].toDouble() - wanted.y()) < 0.000001 &&
            view["panning"].toBool() == dragging;
    };
    const auto navigation = [&](int percent) {
        return circuit_->zoomPercent() == percent && circuit_zoom_note_->text() == "Zoom: " + QString::number(percent) + "%" &&
            circuit_zoom_in_->isEnabled() == (percent < 150) && circuit_zoom_out_->isEnabled() == (percent > 50) &&
            circuit_fit_->isEnabled();
    };
    const auto click = [&](const QString& id) {
        // Literal fixture geometry and expected pan; never componentPoint or the reported rectangle.
        send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, project(id == "r" ? QPointF(255,190) : QPointF(445,280)));
        const auto context = circuit_context_->currentData().toStringList();
        return circuit_->selection() == QStringList{context[0], context[1], id};
    };
    const auto choose = [&](const QString& side) {
        circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main", side})); settle();
    };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()},
            {"edit_target", target}, {"r_draft", resistance_->text()}, {"c_draft", capacitance_->text()},
            {"r_active", activeR()}, {"c_active", activeC()}, {"properties_text", circuit_selection_->text()}});
    };
    checks["empty_wheel_ignored_no_view_change"] = !wheel({0,120}) && circuit_->zoomPercent() == 100 && isPan({0,0}) && !circuit_->supported();
    checks["inert_open_centered"] = openDocument(root,"original.json") && circuit_->supported() && navigation(100) &&
        isPan({0,0}) && circuit_->selection().isEmpty() && selected_instance_.isEmpty() && !document_.dirty();
    settle(); observe("original", "original.json");
    const auto original = document_.graph();
    checks["positive_detent_updates_buttons_percentage"] = wheel({0,120}) && navigation(125) && isPan({0,0}) && document_.graph() == original;
    observe("wheel125", "original.json");
    const auto upper = wheel({0,120}) && wheel({0,120}) && navigation(150);
    click("r"); beginCircuitResistanceEdit();
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    checks["repeated_detent_upper_bound_and_R_hit"] = upper && activeR() && drafts() && isPan({0,0}) && document_.graph() == original;
    observe("wheel150_R", "original.json");
    checks["full_burst_limited_lower_bound"] = wheel({0,std::numeric_limits<int>::min()}) && navigation(50) &&
        wheel({0,-120}) && navigation(50) && activeR() && drafts() && document_.graph() == original && grab().save(report + ".small.png");
    observe("wheel50_R", "original.json");
    press();
    checks["partial_and_zero_deltas_ignored"] = !wheel({0,60}) && !wheel({0,60}) && !wheel({0,-60}) && !wheel({0,0}) &&
        navigation(50) && activeR() && drafts() && isPan({0,0},true) && circuit_->cursor().shape() == Qt::ClosedHandCursor;
    checks["horizontal_pixel_inverted_phase_modified_buttons_ignored"] =
        !wheel({120,0}) && !wheel({120,120}) && !wheel({0,120},{0,12}) && !wheel({0,120},{},Qt::NoScrollPhase,true) &&
        !wheel({0,120},{},Qt::ScrollUpdate) && !wheel({0,120},{},Qt::NoScrollPhase,false,Qt::ControlModifier) &&
        !wheel({0,120},{},Qt::NoScrollPhase,false,Qt::ShiftModifier) &&
        !wheel({0,120},{},Qt::NoScrollPhase,false,Qt::NoModifier,Qt::LeftButton) && navigation(50) && drafts() && activeR() && isPan({0,0},true);
    send(QEvent::MouseButtonRelease, Qt::MiddleButton, Qt::NoButton, mouse_position);
    checks["negative_detent_changes_one_step"] = wheel({0,120}) && navigation(75) && wheel({0,-120}) && navigation(50) && drafts();
    wheel({0,360}); click("c"); beginCircuitCapacitanceEdit();
    observe("wheel125_C", "original.json");
    drag({40,-20});
    checks["pan_then_wheel_keeps_offset_drafts_selection"] = navigation(125) && isPan({40,-20}) && activeC() && drafts() &&
        circuit_->selection() == QStringList{"main","right","c"} && document_.graph() == original;
    observe("pan125_C", "original.json");
    press(); move({10,10},true);
    const auto dragged = isPan({50,-10},true);
    const auto changed = wheel({0,120},{},Qt::NoScrollPhase,false,Qt::NoModifier,Qt::MiddleButton);
    move({20,15},false);
    checks["wheel_stops_middle_drag_and_updates_navigation"] = dragged && changed && navigation(150) && isPan({50,-10}) &&
        circuit_->cursor().shape() != Qt::ClosedHandCursor && activeC() && drafts() && grab().save(report + ".large.png");
    observe("wheel150_C", "original.json");
    press(); move({-5,5},true);
    const auto boundary = wheel({0,std::numeric_limits<int>::max()},{},Qt::NoScrollPhase,false,Qt::NoModifier,Qt::MiddleButton);
    move({20,15},false);
    checks["boundary_wheel_cancels_drag_without_view_edit"] = boundary && navigation(150) && isPan({45,-5}) && activeC() && drafts() &&
        document_.graph() == original && circuit_->cursor().shape() != Qt::ClosedHandCursor;
    observe("boundary150_C", "original.json");
    circuit_fit_->click(); expected_pan = {}; settle();
    const auto fitted = navigation(100) && isPan({0,0}) && drafts();
    circuit_zoom_in_->click();
    checks["buttons_and_fit_interoperate_with_wheel"] = fitted && navigation(125) && wheel({0,-120}) && navigation(100) &&
        wheel({0,120}) && navigation(125) && drafts() && activeC();
    drag({35,20}); click("r"); resize(1200,850); settle();
    showDeclarationDetails(true); settle(); showDeclarationDetails(false); settle();
    const auto retained = navigation(125) && isPan({35,20}) && activeR() && drafts();
    choose("left"); click("r");
    const auto stale = !applyCircuitResistance() && !applyCircuitCapacitance() && !activeR() && !activeC() && drafts();
    choose("right"); click("r");
    checks["resize_details_context_preserve_view_refuse_stale_apply"] = retained && stale && activeR() && drafts() &&
        navigation(125) && isPan({35,20}) && document_.graph() == original;
    observe("retained125_R", "original.json");
    name_->setText(text(document_.name())); instance_name_->setText("right"); click("c"); apply_capacitance_->click(); settle();
    const auto C_applied = document_.graph();
    checks["C_apply_via_wheel_preserves_R_draft"] = C_applied != original && navigation(125) && isPan({35,20}) && activeC() &&
        resistance_->text() == "3.5" && capacitance_->text() == "470" && current_occurrence_->parameters.at("capacitance").value == "0.000000470";
    observe("C_applied", "wheel-C-copy.json");
    checks["pending_R_refuses_copy_history"] = !saveCopy("pending-wheel-copy.json") && !undoEdit() && !redoEdit() &&
        document_.graph() == C_applied && navigation(125) && isPan({35,20});
    resistance_->setText("2.2"); const auto C_saved = saveCopy("wheel-C-copy.json");
    click("r"); resistance_->setText("3.5"); apply_resistance_->click(); settle(); const auto both_applied = document_.graph();
    checks["R_apply_via_wheel_preserves_C"] = C_saved && both_applied != C_applied && activeR() && navigation(125) && isPan({35,20}) &&
        resistance_->text() == "3.5" && capacitance_->text() == "470" && current_occurrence_->parameters.at("resistance").value == "3500";
    observe("R_applied", "wheel-RC-copy.json");
    checks["undo_retains_wheel_zoom_pan"] = undoEdit() && navigation(125) && isPan({35,20}) && activeR() && document_.graph() == C_applied &&
        resistance_->text() == "2.2" && capacitance_->text() == "470";
    observe("undo_R", "wheel-C-copy.json");
    checks["redo_retains_wheel_zoom_pan"] = redoEdit() && navigation(125) && isPan({35,20}) && activeR() && document_.graph() == both_applied &&
        resistance_->text() == "3.5" && capacitance_->text() == "470";
    observe("redo_R", "wheel-RC-copy.json");
    checks["create_only_occupied_copy_retains_view"] = saveCopy("wheel-RC-copy.json") && !saveCopy("wheel-RC-copy.json") && navigation(125) &&
        isPan({35,20}) && document_.leaf() == "original.json" && document_.graph() == both_applied && document_.can_undo();
    checks["failed_open_retains_view_drafts"] = !openDocument(root,"invalid.json") && navigation(125) && isPan({35,20}) && activeR() &&
        resistance_->text() == "3.5" && capacitance_->text() == "470" && document_.graph() == both_applied;
    checks["reopen_resets_zoom_pan_selection_activation_history"] = openDocument(root,"wheel-RC-copy.json") && navigation(100) &&
        isPan({0,0}) && circuit_->selection().isEmpty() && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() &&
        selected_instance_.isEmpty() && resistance_->text().isEmpty() && capacitance_->text().isEmpty() && !document_.can_undo() &&
        !document_.can_redo() && !document_.dirty();
    expected_pan = {}; settle(); observe("reopened", "wheel-RC-copy.json");
    wheel({0,240}); drag({20,15});
    const auto unsupported = openDocument(root,"changed-net.json"); expected_pan = {};
    checks["unsupported_wheel_ignored_reset_view"] = unsupported && !wheel({0,120}) && !circuit_->supported() &&
        circuit_->zoomPercent() == 100 && isPan({0,0}) && !circuit_zoom_in_->isEnabled() && !circuit_fit_->isEnabled() && circuit_->selection().isEmpty();
    observe("unsupported", "changed-net.json");
    openDocument(root,"wheel-RC-copy.json"); wheel({0,-240});
    const auto R_hit = navigation(50) && click("r") && beginCircuitResistanceEdit() && activeR();
    wheel({0,360});
    checks["wheel_hits_R_C_with_same_geometry"] = R_hit && navigation(125) && click("c") && beginCircuitCapacitanceEdit() && activeC() && !document_.dirty();
    checks["source_resources_remain_inert"] = !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    circuit_fit_->click(); expected_pan = {}; click("r"); beginCircuitResistanceEdit(); settle(); showAnalyzer(); settle();
    checks["final_fit_independent_windows_panels_images"] = navigation(100) && isPan({0,0}) && activeR() && !document_.dirty() &&
        analyzer_.isWindow() && analyzer_.isVisible() && isWindow() && components_->features().testFlag(QDockWidget::DockWidgetClosable) &&
        properties_->features().testFlag(QDockWidget::DockWidgetMovable) && grab().save(report + ".fit.png");
    observe("final_fit_R", "wheel-RC-copy.json");
    bool passed = checks.size() == 25;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "Bounded complete vertical wheel detents on fixed RC view; no accumulated partial/pixel/phase/inverted/modified gestures, cursor anchoring, physical mouse, keyboard, DPI/session or packaging acceptance"}};
    const auto output = QJsonDocument(result).toJson();
    QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout);
    QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runFocusAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations, roundtrips;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto activeR = [this] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [this] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    const auto toggle = [&] { focus_action_->trigger(); settle(); };
    const auto layout = [&] {
        QJsonArray sizes, geometry;
        for(const auto value : catalog_splitter_->sizes()) sizes.append(value);
        const auto rect = properties_->geometry();
        for(const auto value : {rect.x(),rect.y(),rect.width(),rect.height()}) geometry.append(value);
        return QJsonObject{{"components_hidden",components_->isHidden()}, {"properties_hidden",properties_->isHidden()},
            {"components_floating",components_->isFloating()}, {"properties_floating",properties_->isFloating()},
            {"components_width",components_->width()}, {"properties_width",properties_->width()},
            {"components_area",static_cast<int>(dockWidgetArea(components_))}, {"properties_area",static_cast<int>(dockWidgetArea(properties_))},
            {"splitter",sizes}, {"properties_geometry",geometry}};
    };
    const auto equalLayout = [&](const QJsonObject& before, const QJsonObject& after, bool floating) {
        for(const auto* key : {"components_hidden","properties_hidden","components_floating","properties_floating","components_area","properties_area"})
            if(before[key] != after[key]) return false;
        for(const auto* side : {"components","properties"}) {
            const auto hidden = QString(side) + "_hidden", width = QString(side) + "_width";
            if(!before[hidden].toBool() && std::abs(before[width].toInt() - after[width].toInt()) > 2) return false;
        }
        const auto first = before["splitter"].toArray(), second = after["splitter"].toArray();
        if(first.size() != second.size()) return false;
        for(qsizetype i = 0; i < first.size(); ++i) if(std::abs(first[i].toInt() - second[i].toInt()) > 2) return false;
        if(floating) {
            const auto before_geometry = before["properties_geometry"].toArray(), after_geometry = after["properties_geometry"].toArray();
            for(qsizetype i = 0; i < before_geometry.size(); ++i) if(std::abs(before_geometry[i].toInt() - after_geometry[i].toInt()) > 2) return false;
        }
        return true;
    };
    const auto recordLayout = [&](const QString& name, const QJsonObject& before, bool floating = false) {
        const auto after = layout();
        roundtrips.append(QJsonObject{{"name",name},{"before",before},{"after",after}});
        return equalLayout(before,after,floating);
    };
    const auto isPan = [&](const QPointF& wanted, bool dragging = false) {
        const auto view = circuit_->snapshot()["view"].toObject(); const auto pan = view["pan"].toArray();
        return std::abs(pan[0].toDouble() - wanted.x()) < 0.000001 && std::abs(pan[1].toDouble() - wanted.y()) < 0.000001 &&
            view["panning"].toBool() == dragging;
    };
    const QPointF expected_pan(35,20);
    const auto scale = [this] {
        return std::min((circuit_->width() - 24.0) / 700, (circuit_->height() - 24.0) / 420) * circuit_->zoomPercent() / 100.0;
    };
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, const QPointF& point) {
        QMouseEvent event(type, point, QPointF(circuit_->mapToGlobal(point.toPoint())), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(circuit_,&event); settle();
    };
    const auto click = [&](const QString& id, QPointF offset = QPointF(35,20)) {
        settle();
        const auto center = id == "r" ? QPointF(255,190) : QPointF(445,280);
        const auto point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0) + (center - QPointF(350,210) + offset) * scale();
        send(QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton,point);
        return circuit_->selection() == QStringList{"main","right",id};
    };
    const auto array = [](const QStringList& values) { return QJsonArray::fromStringList(values); };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage",stage},{"input",input},{"canvas",circuit_->snapshot()},
            {"edit_target",target},{"r_draft",resistance_->text()},{"c_draft",capacitance_->text()},
            {"r_active",activeR()},{"c_active",activeC()},{"focused",circuit_focused_},
            {"r_activation",array(circuit_resistance_target_)},{"c_activation",array(circuit_capacitance_target_)},
            {"project_draft",name_->text()},{"instance_draft",instance_name_->text()},
            {"properties_text",circuit_selection_->text()}});
    };
    toggle(); const auto empty = circuit_focused_ && components_->isHidden() && properties_->isHidden(); toggle();
    checks["empty_focus_roundtrip_inert"] = empty && !circuit_focused_ && !document_.graph() && circuit_->zoomPercent() == 100 && isPan({0,0});
    checks["inert_open_default_view"] = openDocument(root,"original.json") && circuit_->supported() && !circuit_focused_ &&
        circuit_->selection().isEmpty() && selected_instance_.isEmpty() && !document_.dirty() && isPan({0,0});
    settle(); observe("original","original.json");
    const auto original = document_.graph();
    circuit_zoom_in_->click();
    auto point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0);
    settle(); send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point);
    point += expected_pan * scale(); send(QEvent::MouseMove,Qt::NoButton,Qt::MiddleButton,point);
    send(QEvent::MouseButtonRelease,Qt::MiddleButton,Qt::NoButton,point);
    click("r"); beginCircuitResistanceEdit();
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [this] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    resizeDocks({components_,properties_},{300,500},Qt::Horizontal); catalog_splitter_->setSizes({200,350}); settle();
    checks["R_zoom_pan_four_drafts"] = circuit_->zoomPercent() == 125 && isPan(expected_pan) && activeR() && drafts() && document_.graph() == original;
    observe("normal_R","original.json");
    const auto initial = layout(); const auto width = circuit_->width();
    const auto normal_image = grab().save(report + ".normal.png");
    toggle();
    checks["focus_hides_panels_expands_canvas"] = circuit_focused_ && focus_action_->isChecked() && focus_action_->text() == "Restore Panels" &&
        components_->isHidden() && properties_->isHidden() && circuit_->width() > width + 300;
    showDeclarationDetails(true);
    checks["focus_blocks_details_panel_toggles"] = !details_action_->isEnabled() && !details_action_->isChecked() &&
        !components_->toggleViewAction()->isEnabled() && !properties_->toggleViewAction()->isEnabled() &&
        focus_action_->isEnabled() && centralWidget() == circuit_panel_;
    checks["focus_preserves_drafts_graph_view"] = drafts() && document_.graph() == original && circuit_->zoomPercent() == 125 &&
        isPan(expected_pan) && circuit_->selection() == QStringList{"main","right","r"} && circuit_resistance_target_ == QStringList{"main","right"};
    observe("focus_R","original.json");
    checks["focus_R_C_hit_inspection_only"] = click("r") && click("c") && !activeR() && !activeC() && drafts() && document_.graph() == original;
    observe("focus_C","original.json");
    showAnalyzer(); settle(); const auto geometry = analyzer_.geometry(); analyzer_.close(); showAnalyzer(); settle();
    checks["analyzer_independent_close_reopen"] = analyzer_.isWindow() && analyzer_.isVisible() && isWindow() &&
        analyzer_.geometry() == geometry && circuit_focused_ && drafts() && document_.graph() == original;
    const auto focus_image = grab().save(report + ".focus.png");
    click("r"); toggle();
    checks["restores_panels_sizes_splitter_R_activation"] = recordLayout("initial",initial) && !circuit_focused_ && activeR() && drafts() &&
        details_action_->isEnabled() && components_->toggleViewAction()->isEnabled() && properties_->toggleViewAction()->isEnabled() &&
        focus_action_->text() == "Focus Circuit" && !focus_action_->isChecked() && isPan(expected_pan) && circuit_->zoomPercent() == 125;
    observe("restored_R","original.json");
    resizeDocks({components_,properties_},{380,560},Qt::Horizontal); catalog_splitter_->setSizes({350,200}); settle();
    const auto resized = layout(); toggle(); toggle();
    checks["repeat_cycle_uses_new_panel_sizes"] = recordLayout("resized",resized) && resized != initial && drafts();
    components_->hide(); settle(); const auto hidden = layout(); toggle(); toggle();
    checks["previously_hidden_components_remain_hidden"] = recordLayout("hidden_components",hidden) && components_->isHidden() && !properties_->isHidden() && drafts();
    properties_->hide(); settle(); const auto both_hidden = layout(); toggle(); toggle();
    checks["both_previously_hidden_remain_hidden"] = recordLayout("both_hidden",both_hidden) && components_->isHidden() && properties_->isHidden() && drafts();
    components_->show(); properties_->show(); properties_->setFloating(true); properties_->resize(500,700); properties_->move(40,80); settle();
    const auto floating = layout(); toggle(); toggle();
    checks["floating_properties_visibility_geometry_restored"] = recordLayout("floating_properties",floating,true) && properties_->isFloating() &&
        !properties_->isHidden() && !components_->isHidden() && drafts() && document_.graph() == original;
    properties_->setFloating(false); addDockWidget(Qt::RightDockWidgetArea,properties_); settle();
    showDeclarationDetails(true); settle();
    checks["details_refuses_focus"] = !focus_action_->isEnabled() && !setCircuitFocus(true) && !circuit_focused_ &&
        details_action_->isChecked() && centralWidget() == declaration_panel_ && drafts();
    showDeclarationDetails(false); settle();
    point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0);
    send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point); const auto dragging = isPan(expected_pan,true);
    toggle(); const auto cancelled = isPan(expected_pan) && circuit_->cursor().shape() != Qt::ClosedHandCursor; toggle();
    checks["focus_toggle_during_drag_cancels_view_only"] = dragging && cancelled && isPan(expected_pan) && drafts() &&
        activeR() && circuit_->zoomPercent() == 125 && document_.graph() == original;
    name_->setText(text(document_.name())); instance_name_->setText("right"); click("c"); beginCircuitCapacitanceEdit();
    apply_capacitance_->click(); settle(); const auto applied = document_.graph();
    checks["C_apply_retains_R_draft_view"] = applied != original && activeC() && resistance_->text() == "3.5" && capacitance_->text() == "470" &&
        circuit_->zoomPercent() == 125 && isPan(expected_pan);
    observe("C_applied","focus-C-copy.json");
    checks["pending_R_refuses_copy_history"] = !saveCopy("pending-focus-copy.json") && !undoEdit() && !redoEdit() &&
        document_.graph() == applied && resistance_->text() == "3.5" && isPan(expected_pan);
    resistance_->setText("2.2"); toggle(); observe("focus_C_applied","focus-C-copy.json");
    const auto undone = undoEdit() && document_.graph() == original && capacitance_->text() == "220" && circuit_focused_ && isPan(expected_pan);
    observe("undo_focus_C","original.json");
    const auto redone = redoEdit() && document_.graph() == applied && capacitance_->text() == "470" && circuit_focused_ && isPan(expected_pan);
    checks["focus_undo_redo_retains_layout_view_C"] = undone && redone && resistance_->text() == "2.2" && circuit_->zoomPercent() == 125 &&
        components_->isHidden() && properties_->isHidden();
    observe("redo_focus_C","focus-C-copy.json");
    checks["focus_create_only_copy_and_occupied_refusal"] = saveCopy("focus-C-copy.json") && !saveCopy("focus-C-copy.json") &&
        circuit_focused_ && document_.leaf() == "original.json" && document_.graph() == applied && isPan(expected_pan);
    checks["failed_open_retains_focus_selection_drafts"] = !openDocument(root,"invalid.json") && circuit_focused_ &&
        document_.graph() == applied && circuit_->selection() == QStringList{"main","right","c"} &&
        resistance_->text() == "2.2" && capacitance_->text() == "470" && isPan(expected_pan);
    checks["successful_open_in_focus_resets_document_view_only"] = openDocument(root,"focus-C-copy.json") && circuit_focused_ &&
        components_->isHidden() && properties_->isHidden() && circuit_->selection().isEmpty() && selected_instance_.isEmpty() &&
        circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() && resistance_->text().isEmpty() && capacitance_->text().isEmpty() &&
        isPan({0,0}) && circuit_->zoomPercent() == 100 && !document_.dirty() && !document_.can_undo() && !document_.can_redo();
    settle(); observe("reopened_focus","focus-C-copy.json");
    checks["unsupported_focus_remains_inert"] = openDocument(root,"changed-net.json") && circuit_focused_ && !circuit_->supported() &&
        !circuit_->zoomIn() && !circuit_->zoomOut() && isPan({0,0}) && !document_.dirty() && components_->isHidden() && properties_->isHidden();
    observe("unsupported_focus","changed-net.json");
    openDocument(root,"focus-C-copy.json"); toggle(); click("c",{}); beginCircuitCapacitanceEdit(); settle();
    checks["leave_focus_restores_adjustable_panels_and_images"] = !circuit_focused_ && !components_->isHidden() && !properties_->isHidden() &&
        activeC() && circuit_->zoomPercent() == 100 && isPan({0,0}) && !document_.dirty() && normal_image && focus_image &&
        components_->features().testFlag(QDockWidget::DockWidgetMovable) && properties_->features().testFlag(QDockWidget::DockWidgetClosable) &&
        grab().save(report + ".restored.png");
    observe("final_restored_C","focus-C-copy.json");
    checks["resources_unaccessed_source_unchanged"] = !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1 && document_.leaf() == "focus-C-copy.json";
    bool passed = checks.size() == 24;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed",passed},{"checks",checks},{"observations",observations},{"layout_roundtrips",roundtrips},
        {"qt_version",qVersion()},{"platform",QApplication::platformName()},
        {"scope","Transient same-session Focus Circuit on fixed RC view; no physical user recovery, keyboard/accessibility, monitor/DPI, cross-session layout or packaging acceptance"}};
    const auto output = QJsonDocument(result).toJson(); QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(),1,static_cast<std::size_t>(output.size()),stdout); QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runKeyboardAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations, routed_events;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto owner = [&](QWidget* widget) -> QString {
        if(widget == circuit_) return "canvas";
        if(widget == resistance_) return "resistance";
        if(widget == capacitance_) return "capacitance";
        if(widget == name_) return "project";
        if(widget == instance_name_) return "instance";
        if(widget && widget->window() == &analyzer_) return "analyzer";
        return "other";
    };
    const auto focus = [&](QWidget* widget) { widget->window()->activateWindow(); widget->setFocus(Qt::OtherFocusReason); settle(); };
    const auto key = [&](const QString& label, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                         bool repeat = false, const QString& text_value = QString{}) {
        auto* receiver = QApplication::focusWidget();
        const auto before = owner(receiver); const auto percent = circuit_->zoomPercent();
        auto* field = qobject_cast<QLineEdit*>(receiver);
        const auto cursor = field ? field->cursorPosition() : -1;
        QKeyEvent press(QEvent::KeyPress,code,modifiers,text_value,repeat);
        if(receiver) QApplication::sendEvent(receiver,&press);
        const auto accepted = press.isAccepted();
        QKeyEvent release(QEvent::KeyRelease,code,modifiers,text_value,repeat);
        if(receiver) QApplication::sendEvent(receiver,&release);
        settle();
        routed_events.append(QJsonObject{{"case",label},{"receiver",before},{"focus_before",before},
            {"focus_after",owner(QApplication::focusWidget())},{"key",code},{"modifiers",static_cast<int>(modifiers)},
            {"repeat",repeat},{"zoom_before",percent},{"zoom_after",circuit_->zoomPercent()},
            {"cursor_before",cursor},{"cursor_after",field ? field->cursorPosition() : -1},{"accepted",accepted}});
    };
    const auto isPan = [&](QPointF wanted, bool dragging = false) {
        const auto view = circuit_->snapshot()["view"].toObject(); const auto pan = view["pan"].toArray();
        return std::abs(pan[0].toDouble()-wanted.x()) < 0.000001 && std::abs(pan[1].toDouble()-wanted.y()) < 0.000001 &&
            view["panning"].toBool() == dragging;
    };
    const auto scale = [&] { return std::min((circuit_->width()-24.0)/700,(circuit_->height()-24.0)/420)*circuit_->zoomPercent()/100.0; };
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, QPointF point) {
        QMouseEvent event(type,point,QPointF(circuit_->mapToGlobal(point.toPoint())),button,buttons,Qt::NoModifier);
        QApplication::sendEvent(circuit_,&event); settle();
    };
    const auto click = [&](const QString& id, QPointF pan = QPointF{}) {
        const auto center = id == "r" ? QPointF(255,190) : QPointF(445,280);
        const auto point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0)+(center-QPointF(350,210)+pan)*scale();
        send(QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton,point);
        send(QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton,point);
        return circuit_->selection() == QStringList{"main","right",id};
    };
    const auto activeR = [&] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [&] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        observations.append(QJsonObject{{"stage",stage},{"input",input},{"canvas",circuit_->snapshot()},
            {"edit_target",target},{"r_draft",resistance_->text()},{"c_draft",capacitance_->text()},
            {"r_active",activeR()},{"c_active",activeC()},{"focused",circuit_focused_},
            {"r_activation",QJsonArray::fromStringList(circuit_resistance_target_)},
            {"c_activation",QJsonArray::fromStringList(circuit_capacitance_target_)},
            {"project_draft",name_->text()},{"instance_draft",instance_name_->text()},
            {"properties_text",circuit_selection_->text()},{"focus_owner",owner(QApplication::focusWidget())}});
    };
    focus(circuit_); key("empty_home",Qt::Key_Home); key("empty_up",Qt::Key_PageUp); key("empty_down",Qt::Key_PageDown);
    checks["empty_keys_inert"] = !document_.graph() && circuit_->zoomPercent() == 100 && isPan({0,0});
    checks["inert_open_default_view"] = openDocument(root,"original.json") && circuit_->supported() && !document_.dirty() &&
        circuit_->selection().isEmpty() && isPan({0,0});
    settle(); focus(circuit_); observe("original","original.json");
    const auto original = document_.graph();
    circuit_fit_->setFocus(); settle();
    const auto hit_R = click("r");
    checks["canvas_mouse_focus_indicator"] = hit_R && circuit_->hasFocus() && circuit_->focusPolicy() == Qt::StrongFocus;
    beginCircuitResistanceEdit();
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [&] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    focus(circuit_); observe("R_drafts","original.json");
    key("canvas_page_up",Qt::Key_PageUp); const auto step = circuit_->zoomPercent() == 125;
    key("canvas_repeat_up",Qt::Key_PageUp,Qt::NoModifier,true); const auto upper = circuit_->zoomPercent() == 150;
    key("upper_bound",Qt::Key_PageUp);
    const auto upper_bound = circuit_->zoomPercent() == 150 && !circuit_zoom_in_->isEnabled();
    key("canvas_page_down",Qt::Key_PageDown);
    for(int i = 0; i < 3; ++i) key("lower_step_"+QString::number(i),Qt::Key_PageDown,Qt::NoModifier,i == 2);
    key("lower_bound",Qt::Key_PageDown);
    checks["page_up_down_bounds_repeat"] = step && upper && upper_bound && circuit_->zoomPercent() == 50 &&
        !circuit_zoom_out_->isEnabled() && drafts() && document_.graph() == original;
    observe("lower_bound","original.json");
    key("canvas_home",Qt::Key_Home);
    checks["home_recenter"] = circuit_->zoomPercent() == 100 && isPan({0,0}) && drafts() && activeR();
    observe("reset_drafts","original.json");
    key("modified_canvas",Qt::Key_PageUp,Qt::ControlModifier); key("unknown_canvas",Qt::Key_End);
    checks["modified_unknown_keys_inert"] = circuit_->zoomPercent() == 100 && isPan({0,0}) && drafts() && document_.graph() == original;
    key("tab_leave",Qt::Key_Tab); const auto left_canvas = !circuit_->hasFocus();
    key("tab_return",Qt::Key_Tab,Qt::ShiftModifier);
    checks["tab_roundtrip_native"] = left_canvas && circuit_->hasFocus() && drafts();
    focus(resistance_); resistance_->setCursorPosition(3); key("field_R_home",Qt::Key_Home);
    const auto cursor_home = QApplication::focusWidget() == resistance_ && resistance_->cursorPosition() == 0;
    key("field_R_text",Qt::Key_9,Qt::NoModifier,false,"9");
    const auto inserted = resistance_->text() == "93.5" && resistance_->cursorPosition() == 1;
    resistance_->setText("3.5"); resistance_->setCursorPosition(3);
    const auto field_image = grab().save(report+".field.png");
    key("field_R_page_up",Qt::Key_PageUp); key("field_R_page_down",Qt::Key_PageDown);
    const auto field_pages = circuit_->zoomPercent() == 100 && isPan({0,0}) && drafts();
    showDeclarationDetails(true); settle();
    bool local_homes = cursor_home && inserted && field_image;
    for(auto* field : {capacitance_,name_,instance_name_}) {
        focus(field); field->setCursorPosition(static_cast<int>(field->text().size()));
        const auto label = field == capacitance_ ? "field_C_home" : field == name_ ? "field_project_home" : "field_instance_home";
        key(label,Qt::Key_Home);
        local_homes = local_homes && QApplication::focusWidget() == field && field->cursorPosition() == 0;
    }
    focus(name_); key("details_page",Qt::Key_PageUp);
    checks["field_home_and_text_local"] = local_homes && circuit_->zoomPercent() == 100 && drafts() && document_.graph() == original;
    checks["field_page_keys_no_zoom"] = field_pages && circuit_->zoomPercent() == 100 && isPan({0,0}) && drafts();
    showDeclarationDetails(false); settle(); focus(circuit_);
    auto point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0);
    send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point);
    const auto dragging = isPan({0,0},true); focus(resistance_);
    checks["focus_loss_cancels_drag"] = dragging && isPan({0,0}) && circuit_->cursor().shape() != Qt::ClosedHandCursor && drafts();
    focus(circuit_);
    send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point);
    point += QPointF(35,20)*scale(); send(QEvent::MouseMove,Qt::NoButton,Qt::MiddleButton,point);
    key("drag_ignored",Qt::Key_PageUp,Qt::ControlModifier); const auto retained_drag = isPan({35,20},true);
    key("drag_key",Qt::Key_PageUp);
    send(QEvent::MouseMove,Qt::NoButton,Qt::MiddleButton,point+QPointF(80,50));
    send(QEvent::MouseButtonRelease,Qt::MiddleButton,Qt::NoButton,point);
    checks["keyboard_during_drag_cancels_retains_pan"] = retained_drag && circuit_->zoomPercent() == 125 && isPan({35,20}) && drafts();
    observe("panned_key","original.json");
    checks["selection_hit_after_keyboard_zoom"] = click("c",{35,20}) && !activeR() && !activeC() && drafts() && document_.graph() == original;
    observe("keyboard_C","original.json");
    click("r",{35,20}); focus_action_->trigger(); settle(); focus(circuit_);
    key("focus_mode_home",Qt::Key_Home); key("focus_mode_up",Qt::Key_PageUp);
    observe("focused_keys","original.json");
    const auto focused = circuit_focused_ && components_->isHidden() && properties_->isHidden() && isPan({0,0}) &&
        circuit_->zoomPercent() == 125 && drafts();
    showAnalyzer(); settle(); focus(&analyzer_);
    key("analyzer_home",Qt::Key_Home); key("analyzer_up",Qt::Key_PageUp);
    checks["independent_analyzer_keys_no_zoom"] = analyzer_.isWindow() && analyzer_.isVisible() && isWindow() &&
        owner(QApplication::focusWidget()) == "analyzer" && circuit_->zoomPercent() == 125 && isPan({0,0}) && drafts();
    analyzer_.close(); activateWindow(); focus_action_->trigger(); settle(); focus(circuit_);
    checks["focus_mode_keyboard_restore_drafts"] = focused && !circuit_focused_ && activeR() && drafts() &&
        circuit_->zoomPercent() == 125 && isPan({0,0}) && document_.graph() == original;
    observe("restored_R","original.json");
    name_->setText(text(document_.name())); instance_name_->setText("right"); capacitance_->setText("220");
    apply_resistance_->click(); settle(); focus(circuit_); const auto applied = document_.graph();
    checks["R_apply_retains_view"] = applied != original && activeR() && resistance_->text() == "3.5" &&
        circuit_->zoomPercent() == 125 && isPan({0,0});
    const auto undone = undoEdit() && document_.graph() == original && resistance_->text() == "2.2" && circuit_->zoomPercent() == 125;
    const auto redone = redoEdit() && document_.graph() == applied && resistance_->text() == "3.5" && circuit_->zoomPercent() == 125;
    checks["keyboard_undo_redo_native"] = undone && redone && isPan({0,0}) && activeR();
    focus(circuit_); observe("R_applied","keyboard-R-copy.json");
    checks["create_only_copy_and_occupied_refusal"] = saveCopy("keyboard-R-copy.json") && !saveCopy("keyboard-R-copy.json") &&
        document_.leaf() == "original.json" && document_.graph() == applied;
    checks["failed_open_retains_state"] = !openDocument(root,"invalid.json") && document_.graph() == applied &&
        circuit_->zoomPercent() == 125 && isPan({0,0}) && circuit_->selection() == QStringList{"main","right","r"};
    const auto reopened = openDocument(root,"keyboard-R-copy.json") && !document_.dirty() && circuit_->zoomPercent() == 100 &&
        circuit_->selection().isEmpty() && isPan({0,0}) && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty();
    const auto unsupported = openDocument(root,"changed-net.json") && !circuit_->supported();
    focus(circuit_); key("unsupported_home",Qt::Key_Home); key("unsupported_up",Qt::Key_PageUp); key("unsupported_down",Qt::Key_PageDown);
    checks["reopen_unsupported_details_keys_inert"] = reopened && unsupported && circuit_->zoomPercent() == 100 &&
        isPan({0,0}) && !document_.dirty() && resistance_->text().isEmpty() && capacitance_->text().isEmpty();
    observe("unsupported","changed-net.json");
    checks["resources_unaccessed_source_unchanged"] = !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    openDocument(root,"keyboard-R-copy.json"); settle(); click("r"); beginCircuitResistanceEdit(); focus(circuit_);
    const auto canvas_image = grab().save(report+".canvas.png");
    bool passed = checks.size() == 20 && canvas_image;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed",passed},{"checks",checks},{"observations",observations},{"routed_events",routed_events},
        {"qt_version",qVersion()},{"platform",QApplication::platformName()},
        {"scope","Synthetic focused canvas PgUp/PgDn/Home and native field routing; no full keyboard/accessibility, physical recovery, monitor/DPI, cross-session layout or packaging acceptance"}};
    const auto output = QJsonDocument(result).toJson(); QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(),1,static_cast<std::size_t>(output.size()),stdout); QApplication::exit(passed ? 0 : 1);
}

void DocumentWindow::runDoubleClickAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto focusCanvas = [&] { activateWindow(); circuit_->setFocus(); settle(); };
    const auto owner = [&]() -> QString {
        const auto* widget = QApplication::focusWidget();
        if(widget == circuit_) return "canvas";
        if(widget == resistance_) return "resistance";
        if(widget == capacitance_) return "capacitance";
        return "other";
    };
    const auto activeR = [&] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [&] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    const auto isPan = [&](QPointF wanted, bool dragging = false) {
        const auto view = circuit_->snapshot()["view"].toObject(); const auto pan = view["pan"].toArray();
        return std::abs(pan[0].toDouble()-wanted.x()) < 0.000001 && std::abs(pan[1].toDouble()-wanted.y()) < 0.000001 &&
            view["panning"].toBool() == dragging;
    };
    const auto scale = [&] { return std::min((circuit_->width()-24.0)/700,(circuit_->height()-24.0)/420)*circuit_->zoomPercent()/100.0; };
    const auto pointFor = [&](const QString& id, QPointF pan = QPointF{}) {
        const auto center = id == "r" ? QPointF(255,190) : QPointF(445,280);
        return QPointF(circuit_->width()/2.0,circuit_->height()/2.0)+(center-QPointF(350,210)+pan)*scale();
    };
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, QPointF point,
                          Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent event(type,point,QPointF(circuit_->mapToGlobal(point.toPoint())),button,buttons,modifiers);
        QApplication::sendEvent(circuit_,&event); settle();
    };
    const auto click = [&](const QString& id, QPointF pan = QPointF{}) {
        const auto point = pointFor(id,pan);
        send(QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton,point);
        send(QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton,point);
    };
    const auto doubleClick = [&](const QString& id, QPointF pan = QPointF{}) {
        // One pointer position for the complete native sequence, even if inspection resizes panels.
        const auto point = pointFor(id,pan);
        send(QEvent::MouseButtonPress,Qt::LeftButton,Qt::LeftButton,point);
        send(QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton,point);
        send(QEvent::MouseButtonDblClick,Qt::LeftButton,Qt::LeftButton,point);
        send(QEvent::MouseButtonRelease,Qt::LeftButton,Qt::NoButton,point);
    };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        const auto* field = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        observations.append(QJsonObject{{"stage",stage},{"input",input},{"canvas",circuit_->snapshot()},
            {"edit_target",target},{"r_draft",resistance_->text()},{"c_draft",capacitance_->text()},
            {"r_active",activeR()},{"c_active",activeC()},{"focused",circuit_focused_},
            {"r_activation",QJsonArray::fromStringList(circuit_resistance_target_)},
            {"c_activation",QJsonArray::fromStringList(circuit_capacitance_target_)},
            {"project_draft",name_->text()},{"instance_draft",instance_name_->text()},
            {"properties_text",circuit_selection_->text()},{"focus_owner",owner()},
            {"selected_text",field ? field->selectedText() : QString{}}});
    };
    focusCanvas(); doubleClick("r");
    checks["empty_double_click_inert"] = !document_.graph() && selected_instance_.isEmpty() && !activeR() && !activeC();
    checks["inert_open_no_activation"] = openDocument(root,"original.json") && circuit_->supported() && !document_.dirty() &&
        circuit_->selection().isEmpty() && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty();
    settle(); focusCanvas(); observe("original","original.json"); const auto original = document_.graph();
    click("r");
    checks["single_click_inspection_only"] = circuit_->selection() == QStringList{"main","right","r"} &&
        selected_instance_.isEmpty() && !activeR() && !activeC() && document_.graph() == original;
    observe("inspected_R","original.json");
    doubleClick("r");
    checks["R_double_click_existing_field"] = activeR() && !activeC() && QApplication::focusWidget() == resistance_ &&
        resistance_->selectedText() == "2.2" && circuit_resistance_target_ == QStringList{"main","right"} &&
        document_.graph() == original && !document_.dirty() && grab().save(report+".R.png");
    observe("active_R","original.json");
    circuit_zoom_in_->click(); settle();
    auto point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0);
    send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point);
    point += QPointF(35,20)*scale(); send(QEvent::MouseMove,Qt::NoButton,Qt::MiddleButton,point);
    send(QEvent::MouseButtonRelease,Qt::MiddleButton,Qt::NoButton,point);
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [&] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    doubleClick("r",{35,20});
    checks["repeat_R_preserves_four_drafts"] = activeR() && drafts() && resistance_->selectedText() == "3.5" && document_.graph() == original;
    observe("draft_R","original.json");
    doubleClick("c",{35,20});
    checks["C_double_click_separate_field"] = activeC() && !activeR() && QApplication::focusWidget() == capacitance_ &&
        capacitance_->selectedText() == "470" && drafts() && document_.graph() == original && capacitance_ != resistance_ &&
        circuit_capacitance_target_ == QStringList{"main","right"} && grab().save(report+".C.png");
    observe("active_C","original.json");
    checks["zoom_pan_hit_full_ids"] = circuit_->zoomPercent() == 125 && isPan({35,20}) &&
        circuit_->selection() == QStringList{"main","right","c"};
    const auto targets = [&] { return circuit_resistance_target_ == QStringList{"main","right"} && circuit_capacitance_target_ == QStringList{"main","right"}; };
    focusCanvas();
    send(QEvent::MouseButtonDblClick,Qt::LeftButton,Qt::LeftButton,{10,10});
    send(QEvent::MouseButtonDblClick,Qt::LeftButton,Qt::LeftButton,pointFor("r",{35,20}),Qt::ControlModifier);
    send(QEvent::MouseButtonDblClick,Qt::RightButton,Qt::RightButton,pointFor("r",{35,20}));
    send(QEvent::MouseButtonDblClick,Qt::MiddleButton,Qt::MiddleButton,pointFor("r",{35,20}));
    checks["modified_other_blank_no_activation"] = owner() == "canvas" && targets() && drafts() &&
        circuit_->selection() == QStringList{"main","right","c"} && !activeR() && activeC() && document_.graph() == original;
    point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0);
    send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point);
    send(QEvent::MouseButtonDblClick,Qt::LeftButton,Qt::LeftButton | Qt::MiddleButton,pointFor("r",{35,20}));
    checks["middle_drag_double_click_ignored"] = isPan({35,20},true) && drafts() && targets() && !activeR();
    send(QEvent::MouseButtonRelease,Qt::MiddleButton,Qt::NoButton,point);
    circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main","left"})); settle();
    doubleClick("r",{35,20});
    checks["pending_target_change_refused"] = selected_instance_ == "right" && drafts() && targets() && !activeR() && !activeC() &&
        circuit_->selection() == QStringList{"main","left","r"} && status_->text().contains("pending") && document_.graph() == original;
    observe("left_refused","original.json");
    openDocument(root,"original.json"); settle(); doubleClick("c");
    checks["left_C_missing_literal_refused"] = circuit_context_->currentData().toStringList() == QStringList{"main","left"} &&
        circuit_->selection() == QStringList{"main","left","c"} && selected_instance_.isEmpty() &&
        circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() && !activeR() && !activeC() && !document_.dirty();
    observe("left_uneditable_C","original.json");
    circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main","right"})); settle();
    doubleClick("r"); name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    doubleClick("c"); doubleClick("r"); circuit_zoom_in_->click(); settle();
    point = QPointF(circuit_->width()/2.0,circuit_->height()/2.0);
    send(QEvent::MouseButtonPress,Qt::MiddleButton,Qt::MiddleButton,point);
    point += QPointF(35,20)*scale(); send(QEvent::MouseMove,Qt::NoButton,Qt::MiddleButton,point);
    send(QEvent::MouseButtonRelease,Qt::MiddleButton,Qt::NoButton,point);
    components_->hide(); settle(); focus_action_->trigger(); settle(); const auto focused = circuit_focused_;
    doubleClick("r",{35,20});
    checks["focus_double_click_restores_panels"] = focused && !circuit_focused_ && !focus_action_->isChecked() &&
        components_->isHidden() && !properties_->isHidden() && activeR() && owner() == "resistance" &&
        resistance_->selectedText() == "3.5" && drafts() && targets() && circuit_->zoomPercent() == 125 && isPan({35,20}) &&
        !document_.dirty() && grab().save(report+".restored.png");
    observe("focused_R_restored","original.json");
    properties_->hide(); settle(); doubleClick("c",{35,20});
    checks["closed_properties_explicitly_reopened"] = !properties_->isHidden() && components_->isHidden() && activeC() &&
        owner() == "capacitance" && capacitance_->selectedText() == "470" && drafts() && targets() && isPan({35,20});
    showAnalyzer(); settle(); const auto analyzer_geometry = analyzer_.geometry(); analyzer_.close(); showAnalyzer(); settle();
    checks["independent_analyzer_retained"] = analyzer_.isWindow() && analyzer_.isVisible() && isWindow() && analyzer_.geometry() == analyzer_geometry && drafts();
    analyzer_.close(); activateWindow(); settle();
    name_->setText(text(document_.name())); instance_name_->setText("right"); const auto before_C_apply = document_.graph();
    apply_capacitance_->click(); settle(); const auto C_applied = document_.graph();
    checks["C_apply_R_pending_refusals"] = C_applied != before_C_apply && resistance_->text() == "3.5" && capacitance_->text() == "470" &&
        !saveCopy("pending-double-copy.json") && !undoEdit() && !redoEdit() && circuit_->zoomPercent() == 125 && isPan({35,20});
    focusCanvas(); observe("C_applied","double-C-copy.json"); resistance_->setText("2.2");
    const auto undone = undoEdit() && capacitance_->text() == "220";
    const auto redone = redoEdit() && capacitance_->text() == "470" && document_.graph() == C_applied;
    checks["C_history_then_copy"] = undone && redone && saveCopy("double-C-copy.json") && !saveCopy("double-C-copy.json") && isPan({35,20});
    doubleClick("r",{35,20}); resistance_->setText("3.5"); apply_resistance_->click(); settle(); const auto both = document_.graph();
    checks["R_apply_copy_preserves_C"] = both != C_applied && capacitance_->text() == "470" && resistance_->text() == "3.5" &&
        saveCopy("double-RC-copy.json") && !saveCopy("double-RC-copy.json") && circuit_->zoomPercent() == 125 && isPan({35,20});
    focusCanvas(); observe("R_and_C_applied","double-RC-copy.json");
    const auto failed = !openDocument(root,"invalid.json") && document_.graph() == both && circuit_->zoomPercent() == 125 && isPan({35,20});
    checks["failed_open_reopen_resets"] = failed && openDocument(root,"double-RC-copy.json") && !document_.dirty() &&
        circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() && selected_instance_.isEmpty() &&
        circuit_->selection().isEmpty() && circuit_->zoomPercent() == 100 && isPan({0,0});
    openDocument(root,"changed-net.json"); settle(); doubleClick("r"); doubleClick("c"); focusCanvas();
    checks["unsupported_double_click_inert"] = !circuit_->supported() && circuit_->selection().isEmpty() && !activeR() && !activeC() &&
        selected_instance_.isEmpty() && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() && !document_.dirty();
    observe("unsupported","changed-net.json");
    checks["resources_source_unchanged"] = !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    bool passed = checks.size() == 20;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed",passed},{"checks",checks},{"observations",observations},
        {"qt_version",qVersion()},{"platform",QApplication::platformName()},
        {"scope","Synthetic full Qt double-click sequence on fixed RC and existing native edit; no physical usability/recovery, full keyboard/accessibility, monitor/DPI, cross-session layout or packaging acceptance"}};
    const auto output = QJsonDocument(result).toJson(); QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(),1,static_cast<std::size_t>(output.size()),stdout); QApplication::exit(passed ? 0 : 1);
}
// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
void DocumentWindow::runEnterAcceptance(const QString& root, const QString& report)
{
    QJsonObject checks;
    QJsonArray observations, routed_events;
    const auto settle = [] { for(int i = 0; i < 4; ++i) QApplication::processEvents(); };
    const auto owner = [&](QWidget* widget) -> QString {
        if(widget == circuit_) return "canvas";
        if(widget == resistance_) return "resistance";
        if(widget == capacitance_) return "capacitance";
        return "other";
    };
    const auto focusCanvas = [&] { activateWindow(); circuit_->setFocus(Qt::OtherFocusReason); settle(); };
    const auto key = [&](const QString& label, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                         bool repeat = false, bool direct_canvas = false) {
        auto* press_receiver = direct_canvas ? static_cast<QWidget*>(circuit_) : QApplication::focusWidget();
        const auto before = owner(QApplication::focusWidget());
        const auto percent = circuit_->zoomPercent();
        QKeyEvent press(QEvent::KeyPress, code, modifiers, QString{}, repeat);
        press.ignore();
        if(press_receiver) QApplication::sendEvent(press_receiver, &press);
        const auto accepted = press.isAccepted();
        settle();
        const auto after_press = owner(QApplication::focusWidget());
        // Activation moves focus to the existing editor; release follows that
        // real owner rather than being sent back to the original canvas.
        auto* release_receiver = QApplication::focusWidget();
        QKeyEvent release(QEvent::KeyRelease, code, modifiers, QString{}, repeat);
        if(release_receiver) QApplication::sendEvent(release_receiver, &release);
        settle();
        routed_events.append(QJsonObject{{"case", label}, {"route", direct_canvas ? "direct-negative-probe" : "focus-routed"},
            {"press_receiver", owner(press_receiver)}, {"release_receiver", owner(release_receiver)},
            {"focus_before", before}, {"focus_after_press", after_press}, {"focus_after", owner(QApplication::focusWidget())},
            {"key", code}, {"modifiers", static_cast<int>(modifiers)}, {"repeat", repeat}, {"accepted", accepted},
            {"zoom_before", percent}, {"zoom_after", circuit_->zoomPercent()}});
    };
    const auto activeR = [&] { return resistance_->isVisible() && resistance_->isEnabled() && !details_action_->isChecked(); };
    const auto activeC = [&] { return capacitance_->isVisible() && capacitance_->isEnabled() && !details_action_->isChecked(); };
    const auto isPan = [&](QPointF wanted, bool dragging = false) {
        const auto view = circuit_->snapshot()["view"].toObject(); const auto pan = view["pan"].toArray();
        return std::abs(pan[0].toDouble() - wanted.x()) < 0.000001 && std::abs(pan[1].toDouble() - wanted.y()) < 0.000001 &&
            view["panning"].toBool() == dragging;
    };
    const auto scale = [&] { return std::min((circuit_->width() - 24.0) / 700, (circuit_->height() - 24.0) / 420) * circuit_->zoomPercent() / 100.0; };
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons, QPointF point) {
        QMouseEvent event(type, point, QPointF(circuit_->mapToGlobal(point.toPoint())), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(circuit_, &event); settle();
    };
    const auto click = [&](const QString& id, QPointF pan = QPointF{}) {
        settle();
        const auto center = id == "r" ? QPointF(255,190) : QPointF(445,280);
        const auto point = QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0) + (center - QPointF(350,210) + pan) * scale();
        send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, point);
        send(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton, point);
    };
    const auto panView = [&] {
        settle(); auto point = QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0);
        send(QEvent::MouseButtonPress, Qt::MiddleButton, Qt::MiddleButton, point);
        point += QPointF(35,20) * scale(); send(QEvent::MouseMove, Qt::NoButton, Qt::MiddleButton, point);
        send(QEvent::MouseButtonRelease, Qt::MiddleButton, Qt::NoButton, point);
    };
    const auto observe = [&](const QString& stage, const QString& input) {
        QJsonArray target;
        if(!selected_circuit_.isEmpty()) { target.append(selected_circuit_); target.append(selected_instance_); }
        const auto* field = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        observations.append(QJsonObject{{"stage", stage}, {"input", input}, {"canvas", circuit_->snapshot()},
            {"edit_target", target}, {"r_draft", resistance_->text()}, {"c_draft", capacitance_->text()},
            {"r_active", activeR()}, {"c_active", activeC()}, {"focused", circuit_focused_},
            {"r_activation", QJsonArray::fromStringList(circuit_resistance_target_)},
            {"c_activation", QJsonArray::fromStringList(circuit_capacitance_target_)},
            {"project_draft", name_->text()}, {"instance_draft", instance_name_->text()},
            {"properties_text", circuit_selection_->text()}, {"focus_owner", owner(QApplication::focusWidget())},
            {"selected_text", field ? field->selectedText() : QString{}}});
    };
    focusCanvas(); key("empty_Return", Qt::Key_Return); key("empty_Enter", Qt::Key_Enter);
    const auto empty = !document_.graph() && selected_instance_.isEmpty() && !activeR() && !activeC() && isPan({0,0});
    const auto opened = openDocument(root, "original.json") && circuit_->supported() && !document_.dirty() &&
        circuit_->selection().isEmpty() && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty();
    settle(); focusCanvas(); observe("original", "original.json"); const auto original = document_.graph();
    key("unselected_Return", Qt::Key_Return);
    checks["empty_and_unselected_inert"] = empty && document_.graph() == original && circuit_->selection().isEmpty() && !activeR() && !activeC();
    click("r");
    checks["inert_open_and_single_inspection"] = opened && circuit_->selection() == QStringList{"main","right","r"} &&
        selected_instance_.isEmpty() && !activeR() && !activeC() && document_.graph() == original;
    key("selected_R_Return", Qt::Key_Return);
    checks["Return_activates_existing_R"] = activeR() && !activeC() && owner(QApplication::focusWidget()) == "resistance" &&
        resistance_->selectedText() == "2.2" && circuit_resistance_target_ == QStringList{"main","right"} &&
        document_.graph() == original && !document_.dirty() && grab().save(report + ".R.png");
    observe("active_R", "original.json"); key("field_R_Return", Qt::Key_Return);
    const auto R_field = document_.graph() == original && resistance_->text() == "2.2" && !document_.dirty() && owner(QApplication::focusWidget()) == "resistance";
    click("c"); key("selected_C_Enter", Qt::Key_Enter);
    checks["Enter_activates_existing_C"] = activeC() && !activeR() && owner(QApplication::focusWidget()) == "capacitance" &&
        capacitance_->selectedText() == "220" && circuit_capacitance_target_ == QStringList{"main","right"} && document_.graph() == original;
    observe("active_C", "original.json"); key("field_C_Enter", Qt::Key_Enter);
    checks["native_fields_no_implicit_apply"] = R_field && document_.graph() == original && capacitance_->text() == "220" &&
        !document_.dirty() && owner(QApplication::focusWidget()) == "capacitance";
    focusCanvas(); key("repeat_Return", Qt::Key_Return, Qt::NoModifier, true); key("repeat_Enter", Qt::Key_Enter, Qt::NoModifier, true);
    key("ctrl_Return", Qt::Key_Return, Qt::ControlModifier); key("shift_Enter", Qt::Key_Enter, Qt::ShiftModifier);
    key("keypad_Enter", Qt::Key_Enter, Qt::KeypadModifier); key("unknown_End", Qt::Key_End);
    const auto ignored = owner(QApplication::focusWidget()) == "canvas" && document_.graph() == original && circuit_->zoomPercent() == 100;
    capacitance_->setFocus(); settle(); key("unfocused_canvas_Return", Qt::Key_Return, Qt::NoModifier, false, true);
    const auto unfocused = owner(QApplication::focusWidget()) == "capacitance" && document_.graph() == original;
    showDeclarationDetails(true); settle(); capacitance_->setFocus(); settle();
    key("hidden_canvas_Return", Qt::Key_Return, Qt::NoModifier, false, true);
    checks["repeat_modified_unknown_hidden_unfocused_inert"] = ignored && unfocused && !circuit_->isVisible() &&
        owner(QApplication::focusWidget()) == "capacitance" && document_.graph() == original && !document_.dirty();
    showDeclarationDetails(false); settle(); click("r"); circuit_zoom_in_->click(); settle(); panView();
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    const auto drafts = [&] { return name_->text() == "Pending project" && instance_name_->text() == "Pending right" &&
        resistance_->text() == "3.5" && capacitance_->text() == "470"; };
    const auto targets = [&] { return circuit_resistance_target_ == QStringList{"main","right"} && circuit_capacitance_target_ == QStringList{"main","right"}; };
    focusCanvas(); send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, {10,10});
    send(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton, {10,10}); key("draft_R_Return", Qt::Key_Return);
    checks["four_drafts_targets_retained"] = drafts() && targets() && activeR() && resistance_->selectedText() == "3.5" && document_.graph() == original;
    observe("draft_R", "original.json");
    focusCanvas(); settle(); const auto drag_point = QPointF(circuit_->width() / 2.0, circuit_->height() / 2.0);
    send(QEvent::MouseButtonPress, Qt::MiddleButton, Qt::MiddleButton, drag_point); key("panning_Return", Qt::Key_Return);
    checks["active_middle_drag_refuses"] = isPan({35,20}, true) && drafts() && targets() && owner(QApplication::focusWidget()) == "canvas" && document_.graph() == original;
    send(QEvent::MouseButtonRelease, Qt::MiddleButton, Qt::NoButton, drag_point);
    checks["zoom_pan_context_full_ids"] = circuit_->zoomPercent() == 125 && isPan({35,20}) && circuit_->selection() == QStringList{"main","right","r"};
    circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main","left"})); settle(); click("r", {35,20});
    key("left_R_Return", Qt::Key_Return);
    const auto refused_R = selected_instance_ == "right" && drafts() && targets() && !activeR() && !activeC() &&
        owner(QApplication::focusWidget()) == "canvas" && status_->text().contains("pending") && document_.graph() == original;
    observe("left_refused", "original.json"); openDocument(root, "original.json"); settle(); click("c"); key("left_C_Enter", Qt::Key_Enter);
    checks["pending_left_R_and_missing_left_C_refuse"] = refused_R && circuit_context_->currentData().toStringList() == QStringList{"main","left"} &&
        circuit_->selection() == QStringList{"main","left","c"} && selected_instance_.isEmpty() && !activeR() && !activeC() &&
        circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() && owner(QApplication::focusWidget()) == "canvas" && !document_.dirty();
    observe("left_uneditable_C", "original.json");
    const auto fresh_original = document_.graph();
    circuit_context_->setCurrentIndex(circuit_context_->findData(QStringList{"main","right"})); settle(); click("r"); key("rebuild_R_Return", Qt::Key_Return);
    name_->setText("Pending project"); instance_name_->setText("Pending right"); resistance_->setText("3.5"); capacitance_->setText("470");
    click("c"); key("rebuild_C_Enter", Qt::Key_Enter); click("r"); circuit_zoom_in_->click(); settle(); panView();
    components_->hide(); settle(); focus_action_->trigger(); settle(); const auto was_focused = circuit_focused_;
    focusCanvas(); key("focus_restore_Return", Qt::Key_Return);
    const auto restored = was_focused && !circuit_focused_ && components_->isHidden() && !properties_->isHidden() && activeR() &&
        owner(QApplication::focusWidget()) == "resistance" && resistance_->selectedText() == "3.5" && drafts() && targets();
    properties_->hide(); settle(); click("c", {35,20}); key("closed_properties_Enter", Qt::Key_Enter);
    checks["focus_restore_and_closed_properties_reopen"] = restored && !properties_->isHidden() && components_->isHidden() && activeC() &&
        owner(QApplication::focusWidget()) == "capacitance" && capacitance_->selectedText() == "470" && drafts() && targets() &&
        circuit_->zoomPercent() == 125 && isPan({35,20}) && document_.graph() == fresh_original && grab().save(report + ".restored.png");
    observe("restored_C", "original.json"); name_->setText(text(document_.name())); instance_name_->setText("right");
    const auto before_C_apply = document_.graph(); apply_capacitance_->click(); settle(); const auto C_applied = document_.graph();
    const auto C_changed = C_applied != before_C_apply && capacitance_->text() == "470" && resistance_->text() == "3.5";
    const auto pending = !saveCopy("pending-enter-copy.json") && !undoEdit() && !redoEdit() && document_.graph() == C_applied;
    resistance_->setText("2.2"); const auto undone = undoEdit() && document_.graph() == before_C_apply && capacitance_->text() == "220";
    const auto redone = redoEdit() && document_.graph() == C_applied && capacitance_->text() == "470";
    click("r", {35,20}); key("R_after_C_Return", Qt::Key_Return); resistance_->setText("3.5");
    key("field_R_pending_Return", Qt::Key_Return); const auto field_pending = document_.graph() == C_applied && resistance_->text() == "3.5";
    const auto before_R_apply = document_.graph(); apply_resistance_->click(); settle(); const auto both = document_.graph();
    checks["explicit_apply_history_create_only_copy"] = C_changed && pending && undone && redone && field_pending && both != before_R_apply &&
        capacitance_->text() == "470" && resistance_->text() == "3.5" && saveCopy("enter-RC-copy.json") && !saveCopy("enter-RC-copy.json") &&
        document_.leaf() == "original.json" && circuit_->zoomPercent() == 125 && isPan({35,20});
    focusCanvas(); observe("combined_applied", "enter-RC-copy.json");
    const auto failed_open = !openDocument(root, "invalid.json") && document_.graph() == both && circuit_->zoomPercent() == 125 && isPan({35,20});
    const auto reopened = openDocument(root, "enter-RC-copy.json") && !document_.dirty() && circuit_->selection().isEmpty() &&
        selected_instance_.isEmpty() && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() &&
        circuit_->zoomPercent() == 100 && isPan({0,0}) && !document_.can_undo() && !document_.can_redo();
    const auto unsupported = openDocument(root, "changed-net.json") && !circuit_->supported(); focusCanvas();
    key("unsupported_Return", Qt::Key_Return); key("unsupported_Enter", Qt::Key_Enter);
    checks["failed_open_reopen_unsupported_resources_inert"] = failed_open && reopened && unsupported && !activeR() && !activeC() &&
        selected_instance_.isEmpty() && circuit_resistance_target_.isEmpty() && circuit_capacitance_target_.isEmpty() &&
        resistance_->text().isEmpty() && capacitance_->text().isEmpty() && !document_.dirty() && !artwork_->capture() && !occurrence_capture_ && catalog_->currentRow() == -1;
    observe("unsupported", "changed-net.json");
    bool passed = checks.size() == 13;
    for(const auto value : checks) passed = passed && value.toBool();
    const QJsonObject result{{"passed", passed}, {"checks", checks}, {"observations", observations}, {"routed_events", routed_events},
        {"qt_version", qVersion()}, {"platform", QApplication::platformName()},
        {"scope", "Synthetic focus-routed selected R/C Return and unmodified Qt Enter; direct hidden/unfocused refusal probes disclosed; no physical keypad, full accessibility, recovery, DPI/session or packaging acceptance"}};
    const auto output = QJsonDocument(result).toJson(); QFile file(report);
    if(!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(output) != output.size() || !file.flush()) { QApplication::exit(3); return; }
    std::fwrite(output.constData(), 1, static_cast<std::size_t>(output.size()), stdout); QApplication::exit(passed ? 0 : 1);
}
