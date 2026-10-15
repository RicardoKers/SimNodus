// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "document.hpp"

#include "editing/operations.hpp"
#include "io/project_file.hpp"

#include <QDir>
#include <QFileInfo>
#include <QUndoCommand>
#include <QUndoStack>

namespace simnodus::ui {

class EditCommand : public QUndoCommand {
public:
    EditCommand(Document& document, model::Edit edit)
        : QUndoCommand(QString::fromStdString(edit.description())), document_(document), edit_(std::move(edit))
    {
    }

    void redo() override
    {
        edit_.apply(document_.schematic_);
        emit document_.changed();
    }

    void undo() override
    {
        edit_.revert(document_.schematic_);
        emit document_.changed();
    }

private:
    Document& document_;
    model::Edit edit_;
};

Document::Document(const library::ComponentLibrary& library, QObject* parent)
    : QObject(parent), library_(library), undo_(new QUndoStack(this))
{
    connect(undo_, &QUndoStack::cleanChanged, this, &Document::file_changed);
}

bool Document::apply(model::Edit edit)
{
    if (edit.empty()) {
        return false;
    }
    undo_->push(new EditCommand(*this, std::move(edit)));
    return true;
}

void Document::replace(model::Schematic schematic, const QString& path)
{
    undo_->clear();
    schematic_ = std::move(schematic);
    path_ = path;
    origin_dir_.clear();
    undo_->setClean();
    emit changed();
    emit file_changed();
}

void Document::new_document()
{
    replace({}, {});
}

bool Document::open(const QString& path, QStringList& errors, QStringList& warnings)
{
    // Reading only parses JSON: no code runs and nothing is loaded (PRJ-03).
    auto result = io::load_project(path.toStdWString());
    for (const auto& e : result.errors) {
        errors << QString::fromStdString(e);
    }
    for (const auto& w : result.warnings) {
        warnings << QString::fromStdString(w);
    }
    if (!result.schematic) {
        return false;
    }
    replace(std::move(*result.schematic), path);
    return true;
}

bool Document::open_copy(const QString& path, QStringList& errors, QStringList& warnings)
{
    if (!open(path, errors, warnings)) {
        return false;
    }
    origin_dir_ = QFileInfo(path).absolutePath();
    path_.clear();
    emit file_changed();
    return true;
}

QString Document::base_dir() const
{
    return path_.isEmpty() ? origin_dir_ : QFileInfo(path_).absolutePath();
}

bool Document::save(const QString& path, QString& error)
{
    // Relative file parameters keep pointing at the same files from the new folder.
    const QString old_base = base_dir();
    const QString new_base = QFileInfo(path).absolutePath();
    if (!old_base.isEmpty() && QDir(old_base) != QDir(new_base)) {
        model::Schematic work = schematic_;
        for (const auto& c : schematic_.components()) {
            const auto* d = library_.find(c.type);
            if (!d) {
                continue;
            }
            for (const auto& p : d->parameters) {
                const auto it = c.parameters.find(p.name);
                if (p.kind != library::Parameter::Kind::File || it == c.parameters.end() || it->second.empty()) {
                    continue;
                }
                const QString value = QString::fromStdString(it->second);
                if (QDir::isAbsolutePath(value)) {
                    continue;
                }
                const QString absolute = QDir(old_base).absoluteFilePath(value);
                QString moved = QDir(new_base).relativeFilePath(absolute);
                if (QDir::isAbsolutePath(moved)) { // another drive
                    moved = absolute;
                }
                editing::set_parameter(work, c.id, p.name, moved.toStdString()).apply(work);
            }
        }
        apply(model::make_edit(schematic_, work, "Update file paths"));
    }
    const auto result = io::save_project(schematic_, path.toStdWString());
    if (!result.ok) {
        error = QString::fromStdString(result.error);
        return false;
    }
    path_ = path;
    undo_->setClean();
    emit file_changed();
    return true;
}

QString Document::display_name() const
{
    return path_.isEmpty() ? tr("Untitled") : QFileInfo(path_).fileName();
}

bool Document::modified() const
{
    return !undo_->isClean();
}

} // namespace simnodus::ui
