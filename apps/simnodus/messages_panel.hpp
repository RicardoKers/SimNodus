// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Bottom panel (UI-12): diagnostics and engine messages. Locating the related
// item by double-click arrives with SN-122.
#pragma once

#include <QWidget>

class QTreeWidget;

namespace simnodus::ui {

enum class Severity { Info, Warning, Error };

class MessagesPanel : public QWidget {
    Q_OBJECT
public:
    explicit MessagesPanel(QWidget* parent = nullptr);

    void add(Severity severity, const QString& text, const QString& source = {});
    void clear();
    int count() const;
    int warning_count() const { return warnings_; }
    int error_count() const { return errors_; }

signals:
    void counts_changed(int warnings, int errors);
    void error_added();

private:
    QTreeWidget* list_ = nullptr;
    int warnings_ = 0;
    int errors_ = 0;
};

} // namespace simnodus::ui
