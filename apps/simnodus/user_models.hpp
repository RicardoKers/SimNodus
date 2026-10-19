// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The user's SPICE models (PRJ-07, ADR 0095): `.model` statements and
// subcircuits in *.lib and *.mod files of the model folders the user lists
// (Tools > Model Folders; Documents/SimNodus/models until the user changes it,
// SIMNODUS_MODELS_DIR instead for tests). The files are read as text; a model
// the user chooses is copied into the component, so projects do not depend on
// the folders.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/spice_model.hpp"
#endif

#include <QString>
#include <QStringList>

#include <vector>

namespace simnodus::ui {

struct UserModel {
    library::SpiceModel model;
    QString file; // "folder/file.lib": the folder's name and the file's
};

struct UserSubcircuit {
    library::SpiceSubcircuit subcircuit;
    QString file;
};

// The default folder, Documents/SimNodus/models.
QString default_models_folder();
// The folders read, in order (the setting, or the default folder).
QStringList user_models_folders();
// Sets the folders (from the settings or the Model Folders dialog); an empty
// list means the default folder.
void set_user_models_folders(const QStringList& folders);
// Creates a folder with a short README.txt when it does not exist.
bool ensure_models_folder(const QString& folder);
// Models of every file in the folder, read again when a file changed.
const std::vector<UserModel>& user_models();
// Vendor subcircuits of the same files (ADR 0097).
const std::vector<UserSubcircuit>& user_subcircuits();
// What the last read skipped or changed, "file, line N, MODEL: text", and a
// number that grows with every read (to report each read once).
const std::vector<QString>& user_model_issues();
int user_models_generation();

} // namespace simnodus::ui
