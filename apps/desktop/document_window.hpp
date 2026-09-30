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
class QSplitter;
class QPushButton;

class DocumentWindow final : public QMainWindow {
public:
    DocumentWindow();
    bool openDocument(const QString& root, const QString& leaf);
    bool applyName(const QString& name);
    bool saveCopy(const QString& leaf);
    void showAnalyzer();
    void runAcceptance(const QString& root, const QString& report);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    bool confirmDiscard();
    void refresh();
    void selectCatalog(int row);
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
    QLineEdit* name_;
    QPushButton* apply_;
};
