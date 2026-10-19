// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "user_models.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace simnodus::ui {

namespace {

QStringList configured; // empty: the default folder

} // namespace

QString default_models_folder()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/SimNodus/models");
}

QStringList user_models_folders()
{
    QStringList folders = configured.isEmpty() ? QStringList{default_models_folder()} : configured;
    if (const QString forced = qEnvironmentVariable("SIMNODUS_MODELS_DIR"); !forced.isEmpty()) {
        folders = QStringList{forced}; // tests
    }
    return folders;
}

void set_user_models_folders(const QStringList& folders)
{
    configured.clear();
    for (const auto& f : folders) {
        if (!f.trimmed().isEmpty() && !configured.contains(f)) {
            configured << f;
        }
    }
}

bool ensure_models_folder(const QString& folder)
{
    if (!QDir().mkpath(folder)) {
        return false;
    }
    QFile readme(folder + QStringLiteral("/README.txt"));
    if (!readme.exists() && readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
        readme.write("SimNodus models folder\n"
                     "\n"
                     "Put SPICE model files here (*.lib or *.mod). SimNodus reads their\n"
                     ".model statements of the types NPN, PNP, D, NMOS and PMOS, for\n"
                     "example lines copied from a manufacturer's model file:\n"
                     "\n"
                     "  * BC337-40: NPN, 45 V, 800 mA\n"
                     "  .model BC337-40 NPN(IS=... BF=... VAF=...)\n"
                     "\n"
                     "The comment line just above a statement is shown as its description.\n"
                     "Parameters that are not numbers or not known for the model type are\n"
                     "left out; the Messages panel lists them with the file and line.\n"
                     "Subcircuits (.subckt) of op-amps, transistors, MOSFETs and diodes\n"
                     "work too: Properties > Pins sets which port is which pin.\n"
                     "The models appear in the Model list of those parts in the\n"
                     "Properties panel; a chosen model is saved inside the project.\n"
                     "Check the license of the files you copy here.\n");
    }
    return true;
}

namespace {

std::vector<QString> issues;
std::vector<UserSubcircuit> subcircuits;
int generation = 0;

} // namespace

const std::vector<QString>& user_model_issues()
{
    user_models();
    return issues;
}

const std::vector<UserSubcircuit>& user_subcircuits()
{
    user_models();
    return subcircuits;
}

int user_models_generation()
{
    user_models();
    return generation;
}

const std::vector<UserModel>& user_models()
{
    static std::vector<UserModel> models;
    static QString signature; // folders, file names, sizes and times of the last read
    QFileInfoList files;
    QString now;
    for (const auto& path : user_models_folders()) {
        const QDir folder(path);
        now += QStringLiteral("#") + folder.absolutePath();
        for (const auto& file :
            folder.entryInfoList({QStringLiteral("*.lib"), QStringLiteral("*.mod")}, QDir::Files, QDir::Name)) {
            files << file;
            now += QStringLiteral("|%1:%2:%3")
                       .arg(file.fileName())
                       .arg(file.size())
                       .arg(file.lastModified().toMSecsSinceEpoch());
        }
    }
    if (now == signature) {
        return models;
    }
    signature = now;
    models.clear();
    issues.clear();
    subcircuits.clear();
    ++generation;
    for (const auto& file : files) {
        QFile input(file.absoluteFilePath());
        if (file.size() > 64 * 1024 * 1024 || !input.open(QIODevice::ReadOnly)) {
            continue; // a huge or unreadable file is skipped
        }
        const QByteArray text = input.readAll();
        std::vector<library::ModelIssue> found;
        auto parsed = library::parse_spice_library(
            std::string_view(text.constData(), static_cast<std::size_t>(text.size())), &found);
        const QString name = file.dir().dirName() + QLatin1Char('/') + file.fileName();
        for (auto& m : parsed.models) {
            models.push_back({std::move(m), name});
        }
        for (auto& s : parsed.subcircuits) {
            subcircuits.push_back({std::move(s), name});
        }
        for (const auto& issue : found) {
            issues.push_back(QStringLiteral("%1, line %2, %3: %4")
                                 .arg(name)
                                 .arg(issue.line)
                                 .arg(QString::fromStdString(issue.model), QString::fromStdString(issue.text)));
        }
    }
    return models;
}

} // namespace simnodus::ui
