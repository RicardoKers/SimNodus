// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "user_models.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace simnodus::ui {

QString user_models_folder()
{
    if (const QString forced = qEnvironmentVariable("SIMNODUS_MODELS_DIR"); !forced.isEmpty()) {
        return forced;
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/SimNodus/models");
}

bool ensure_user_models_folder()
{
    const QString folder = user_models_folder();
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
                     "Subcircuits (.subckt) are not used yet. The models appear in the\n"
                     "Model list of transistors, MOSFETs and diodes in the Properties\n"
                     "panel; a chosen model is saved inside the project. Check the\n"
                     "license of the files you copy here.\n");
    }
    return true;
}

const std::vector<UserModel>& user_models()
{
    static std::vector<UserModel> models;
    static QString signature; // file names, sizes and times of the last read
    const QDir folder(user_models_folder());
    const QFileInfoList files =
        folder.entryInfoList({QStringLiteral("*.lib"), QStringLiteral("*.mod")}, QDir::Files, QDir::Name);
    QString now = folder.absolutePath();
    for (const auto& file : files) {
        now += QStringLiteral("|%1:%2:%3")
                   .arg(file.fileName())
                   .arg(file.size())
                   .arg(file.lastModified().toMSecsSinceEpoch());
    }
    if (now == signature) {
        return models;
    }
    signature = now;
    models.clear();
    for (const auto& file : files) {
        QFile input(file.absoluteFilePath());
        if (file.size() > 64 * 1024 * 1024 || !input.open(QIODevice::ReadOnly)) {
            continue; // a huge or unreadable file is skipped
        }
        const QByteArray text = input.readAll();
        for (auto& m : library::parse_spice_models(std::string_view(text.constData(), static_cast<std::size_t>(text.size())))) {
            models.push_back({std::move(m), file.fileName()});
        }
    }
    return models;
}

} // namespace simnodus::ui
