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
    properties_->setWidget(inspector_);
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
    auto* view = menuBar()->addMenu("View");
    view->addAction(components_->toggleViewAction());
    view->addAction(properties_->toggleViewAction());
    auto* analyzer = view->addAction("Open Signal Analyzer");
    connect(analyzer, &QAction::triggered, this, &DocumentWindow::showAnalyzer);
    connect(apply_, &QPushButton::clicked, this, [this] { applyName(name_->text()); });
    connect(catalog_, &QListWidget::currentRowChanged, this, &DocumentWindow::selectCatalog);
    connect(structure_, &QTreeWidget::itemSelectionChanged, this, &DocumentWindow::selectInstance);
    status_ = new QLabel("Open one project declaration. Resource access and simulation are unavailable.");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    statusBar()->addWidget(status_, 1);
    refresh();
}

bool DocumentWindow::confirmDiscard()
{
    const bool unapplied = document_.graph() && name_->text() != text(document_.name());
    if(!document_.dirty() && !unapplied) return true;
    return QMessageBox::question(this, "Discard changes?", "Discard unsaved document/name changes? Save Copy creates a separate file and leaves this document edited.",
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
    status_->setText("Name applied in memory. Use Save Copy explicitly to create a new file.");
    return true;
}
bool DocumentWindow::saveCopy(const QString& leaf)
{
    if(document_.graph() && name_->text() != text(document_.name())) {
        status_->setText("Apply or restore the pending display name before Save Copy.");
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
    catalog_->clear(); structure_->clear();
    inspector_->setText("Select an instance in the declared structure.");
    preview_->setText("Select a declared component definition. Symbol rendering is pending.");
    name_->setEnabled(bool(document_.graph())); apply_->setEnabled(bool(document_.graph()));
    name_->setText(text(document_.name()));
    setWindowTitle("SimNodus Circuit Editor" + (document_.graph() ? " - " + text(document_.name()) : ""));
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
    const auto* item = structure_->currentItem();
    if(!item || !document_.graph()) return;
    const auto circuit_id = item->data(0, Qt::UserRole).toString();
    const auto instance_id = item->data(0, Qt::UserRole + 1).toString();
    inspector_->setText("Select an instance in the declared structure.");
    for(const auto& circuit : document_.graph()->connectivity.circuits) {
        if(text(circuit.identity.id) != circuit_id) continue;
        for(const auto& instance : circuit.instances) {
            if(text(instance.identity.id) != instance_id) continue;
            inspector_->setText("Circuit: " + circuit_id + "\nInstance: " + instance_id + "\nName: " + text(instance.identity.name) + "\nDefinition: " + text(instance.definition) + "\nDeclared properties only; runtime unavailable.");
        }
    }
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
