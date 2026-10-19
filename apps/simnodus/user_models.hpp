// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The user's SPICE models (PRJ-07, ADR 0095): `.model` statements in *.lib and
// *.mod files of a folder (Documents/SimNodus/models, or SIMNODUS_MODELS_DIR).
// The files are read as text; a model the user chooses is copied into the
// component, so projects do not depend on the folder.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/spice_model.hpp"
#endif

#include <QString>

#include <vector>

namespace simnodus::ui {

struct UserModel {
    library::SpiceModel model;
    QString file; // file name in the folder
};

QString user_models_folder();
// Creates the folder with a short README.txt when it does not exist.
bool ensure_user_models_folder();
// Models of every file in the folder, read again when a file changed.
const std::vector<UserModel>& user_models();
// What the last read skipped or changed, "file, line N, MODEL: text", and a
// number that grows with every read (to report each read once).
const std::vector<QString>& user_model_issues();
int user_models_generation();

} // namespace simnodus::ui
