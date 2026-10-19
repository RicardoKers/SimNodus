// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Right column (UI-10, INT-04): edits the selected items and, while a
// simulation runs, shows their live values. Values accept engineering
// notation, are validated inline and apply on Enter or focus-out as one
// undoable change. Common parameters of a multi-selection of one component
// type are edited together.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/component_library.hpp"
#include "model/schematic.hpp"
#endif

#include <QWidget>

#include <set>

class QFormLayout;
class QLabel;
class QWidget;

namespace simnodus::ui {

class Document;

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(Document* document = nullptr, QWidget* parent = nullptr);

    void set_selection(const std::set<model::Id>& ids);
    const std::set<model::Id>& selection() const { return selection_; }
    // Lines shown under "Live values"; empty clears the section.
    void set_live_text(const QString& text);
    void clear_selection();
    QString header_text() const;
    // Puts the keyboard focus in the value field of the selected component.
    void focus_value();

signals:
    void message(const QString& text);
    // The user's models folder was read for a model list (PRJ-07).
    void models_read();
    // Reload of a microcontroller's firmware (restarts a running simulation).
    void firmware_reload_requested();

private:
    void rebuild();
    // Rebuilds after the current event: editors may be inside their own handlers.
    void schedule_rebuild();
    void commit_parameter(const std::string& name, const QString& text);
    QWidget* file_editor(const library::Parameter& parameter, const QString& value);
    QWidget* model_editor(const library::Parameter& parameter, const model::Component& first, const QString& value);
    void commit_model(const std::string& parameter, const std::string& name, const std::string& statement);
    QString project_dir() const;
    void commit_designator(const QString& text);
    void commit_label(const QString& text);
    bool editing() const;

    Document* document_ = nullptr;
    std::set<model::Id> selection_;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QWidget* form_host_ = nullptr;
    QFormLayout* form_ = nullptr;
    QWidget* value_editor_ = nullptr;
    QLabel* live_ = nullptr;
    bool rebuilding_ = false;
    bool rebuild_pending_ = false;
};

} // namespace simnodus::ui
