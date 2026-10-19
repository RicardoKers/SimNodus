// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The open project: schematic, file path and undo history (UI-08, PRJ-02).
// Every change goes through apply(), which pushes the Edit onto the undo
// stack; nothing else modifies the schematic.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/component_library.hpp"
#include "model/edit.hpp"
#include "model/schematic.hpp"

#include <memory>
#include <vector>
#endif

#include <QObject>
#include <QString>
#include <QStringList>

class QUndoStack;

namespace simnodus::ui {

class Document : public QObject {
    Q_OBJECT
public:
    Document(const library::ComponentLibrary& library, QObject* parent = nullptr);

    const model::Schematic& schematic() const { return schematic_; }
    // Only for reserving IDs in operations; changes must still go through apply().
    model::Schematic& id_source() { return schematic_; }
    // The application's parts and this project's blocks (PRJ-06). A change to
    // the blocks makes a new library; the previous ones stay alive, so that
    // definitions the editor holds remain valid.
    const library::ComponentLibrary& library() const { return *library_; }
    // Why some of the project's blocks cannot be used; empty when all can.
    const QStringList& block_problems() const { return block_problems_; }
    QUndoStack* undo_stack() const { return undo_; }

    // Applies and records an edit; returns false for an empty edit.
    bool apply(model::Edit edit);

    void new_document();
    bool open(const QString& path, QStringList& errors, QStringList& warnings);
    // Opens a copy that is not linked to a file (examples).
    bool open_copy(const QString& path, QStringList& errors, QStringList& warnings);
    bool save(const QString& path, QString& error);

    QString file_path() const { return path_; }
    // Folder that relative file parameters (firmware) refer to: the project's
    // folder, or for an example copy the folder it came from. Empty if none.
    QString base_dir() const;
    QString display_name() const;
    bool modified() const;

signals:
    void changed();
    void file_changed();
    void library_changed(); // the project's blocks changed

private:
    friend class EditCommand;
    void replace(model::Schematic schematic, const QString& path);
    void refresh_library();

    const library::ComponentLibrary& base_;
    std::shared_ptr<const library::ComponentLibrary> library_;
    std::vector<std::shared_ptr<const library::ComponentLibrary>> retired_;
    std::map<std::string, model::Block> built_; // the blocks library_ was made from
    QStringList block_problems_;
    model::Schematic schematic_;
    QUndoStack* undo_ = nullptr;
    QString path_;
    QString origin_dir_; // folder of the file an untitled copy came from
};

} // namespace simnodus::ui
