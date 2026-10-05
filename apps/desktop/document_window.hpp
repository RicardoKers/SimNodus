// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/editor_document.hpp"
#include "application/symbol_preview.hpp"
#include <QMainWindow>
#include <QByteArray>
#include <QStringList>

class QDockWidget;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QSplitter;
class QPushButton;
class QAction;
class QComboBox;
class QTableWidget;
class QScrollArea;
class QTabWidget;
class PreviewCanvas;
class RcCanvas;

class DocumentWindow final : public QMainWindow {
public:
    DocumentWindow();
    bool openDocument(const QString& root, const QString& leaf);
    bool applyName(const QString& name);
    bool applyInstanceName(const QString& name);
    bool applyResistance(const QString& value);
    bool applyCapacitance(const QString& value);
    bool undoEdit();
    bool redoEdit();
    bool saveCopy(const QString& leaf);
    void showAnalyzer();
    bool previewArtwork(const QString& resource_root);
    bool captureOccurrence(const QString& resource_root);
    void runAcceptance(const QString& root, const QString& report);
    void runInstanceAcceptance(const QString& root, const QString& report);
    void runHistoryAcceptance(const QString& root, const QString& report);
    void runResistanceAcceptance(const QString& root, const QString& report);
    void runCapacitanceAcceptance(const QString& root, const QString& report);
    void runInspectionAcceptance(const QString& root, const QString& report);
    void runPreviewAcceptance(const QString& root, const QString& resource_root, const QString& report);
    void runPinPreviewAcceptance(const QString& root, const QString& resource_root, const QString& report);
    void runOccurrenceAcceptance(const QString& root, const QString& resource_root, const QString& report);
    void runCanvasAcceptance(const QString& root, const QString& report);
    void runCircuitCapacitanceAcceptance(const QString& root, const QString& report);
    void runCircuitResistanceAcceptance(const QString& root, const QString& report);
    void runZoomAcceptance(const QString& root, const QString& report);
    void runPanAcceptance(const QString& root, const QString& report);
    void runWheelAcceptance(const QString& root, const QString& report);
    void runFocusAcceptance(const QString& root, const QString& report);
    void runKeyboardAcceptance(const QString& root, const QString& report);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    bool confirmDiscard();
    bool instanceDraftPending() const;
    bool resistanceDraftPending() const;
    bool capacitanceDraftPending() const;
    std::optional<simnodus::LiteralValueView> selectedResistance() const;
    std::optional<simnodus::LiteralValueView> selectedCapacitance() const;
    bool editsPending() const;
    bool restoreEdit(bool redo);
    void updateHistoryActions();
    void updateParameterInspection();
    void selectOccurrence();
    void updateInstanceProperties(bool keep_name_draft = false, bool keep_resistance_draft = false,
        bool keep_capacitance_draft = false);
    const simnodus::GraphInstance* selectedInstance() const;
    QTreeWidgetItem* instanceItem(const QString& circuit, const QString& instance) const;
    void refresh();
    void selectCatalog(int row);
    void retainPreview();
    void refreshOccurrenceChoices();
    void updateOccurrenceView();
    void updateLocalEndpoints();
    QStringList peerOccurrencePath() const;
    void updatePeerAction();
    bool inspectSelectedPeer();
    void updateOccurrenceNote(const QString& message = {});
    void updateArtworkNote(const QString& message = {});
    QString selectedComponent() const;
    void selectInstance();
    void showDeclarationDetails(bool enabled);
    bool setCircuitFocus(bool enabled);
    void refreshCircuit();
    bool editContainingRc();
    bool beginCircuitCapacitanceEdit();
    bool circuitCapacitanceEligible() const;
    bool applyCircuitCapacitance();
    bool beginCircuitResistanceEdit();
    bool circuitResistanceEligible() const;
    bool applyCircuitResistance();
    void reportError(const char* operation, const char* code, std::size_t offset,
        std::uint32_t system, bool cleanup = false);
    simnodus::EditorDocument document_;
    QMainWindow analyzer_;
    QDockWidget *components_, *properties_;
    QSplitter* catalog_splitter_;
    QListWidget* catalog_;
    QTreeWidget* structure_;
    QLabel *preview_, *inspector_, *status_, *project_id_;
    QLabel* artwork_note_;
    PreviewCanvas* artwork_;
    QTabWidget* views_;
    QComboBox* occurrence_view_choice_;
    PreviewCanvas* occurrence_artwork_;
    QLabel* occurrence_view_note_;
    QTableWidget* occurrence_terminals_;
    QLabel* local_endpoints_note_;
    QTableWidget* local_endpoints_;
    QPushButton* inspect_peer_;
    QString selected_terminal_;
    std::optional<simnodus::DeclaredLocalNetDetailsView> current_local_net_;
    QPushButton* capture_occurrence_;
    std::optional<simnodus::ComponentOccurrenceView> current_occurrence_;
    std::shared_ptr<const simnodus::OccurrenceArtworkCapture> occurrence_capture_;
    QPushButton* preview_artwork_;
    QLineEdit* name_;
    QLineEdit* instance_name_;
    QLineEdit* resistance_;
    QLineEdit* capacitance_;
    QLabel* resistance_limits_;
    QLabel* capacitance_limits_;
    QPushButton* apply_;
    QPushButton* apply_instance_;
    QPushButton* apply_resistance_;
    QPushButton* apply_capacitance_;
    QAction *undo_, *redo_;
    QComboBox* occurrence_;
    QTableWidget* effective_parameters_;
    QLabel* occurrence_note_;
    QScrollArea* properties_scroll_;
    QString selected_circuit_, selected_instance_;
    RcCanvas* circuit_ = nullptr;
    QWidget *circuit_panel_ = nullptr, *declaration_panel_ = nullptr;
    QWidget *circuit_properties_ = nullptr, *declaration_properties_ = nullptr;
    QComboBox* circuit_context_ = nullptr;
    QLabel* circuit_selection_ = nullptr;
    QPushButton* circuit_edit_ = nullptr;
    QAction* details_action_ = nullptr;
    QAction* focus_action_ = nullptr;
    QByteArray focus_layout_, focus_splitter_;
    bool circuit_focused_ = false;
    QWidget *capacitance_editor_ = nullptr, *capacitance_details_host_ = nullptr, *capacitance_circuit_host_ = nullptr;
    QLabel* circuit_capacitance_target_note_ = nullptr;
    QPushButton* circuit_capacitance_edit_ = nullptr;
    QStringList circuit_capacitance_target_;
    QWidget *resistance_editor_ = nullptr, *resistance_details_host_ = nullptr, *resistance_circuit_host_ = nullptr;
    QLabel* circuit_resistance_target_note_ = nullptr;
    QPushButton* circuit_resistance_edit_ = nullptr;
    QStringList circuit_resistance_target_;
    QPushButton *circuit_zoom_in_ = nullptr, *circuit_zoom_out_ = nullptr, *circuit_fit_ = nullptr;
    QLabel* circuit_zoom_note_ = nullptr;
};
