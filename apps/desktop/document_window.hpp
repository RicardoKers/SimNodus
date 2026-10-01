// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/editor_document.hpp"
#include <QMainWindow>

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
class PreviewCanvas;

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
    void runAcceptance(const QString& root, const QString& report);
    void runInstanceAcceptance(const QString& root, const QString& report);
    void runHistoryAcceptance(const QString& root, const QString& report);
    void runResistanceAcceptance(const QString& root, const QString& report);
    void runCapacitanceAcceptance(const QString& root, const QString& report);
    void runInspectionAcceptance(const QString& root, const QString& report);
    void runPreviewAcceptance(const QString& root, const QString& resource_root, const QString& report);
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
    void updateArtworkNote(const QString& message = {});
    QString selectedComponent() const;
    void selectInstance();
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
};
