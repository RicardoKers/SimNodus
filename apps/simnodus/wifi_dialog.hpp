// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The Wi-Fi network of the project's ESP32-C3 boards (E-09, SN-269):
// SimNodus's access point, the gateway to this computer's network (off by
// default, decision DE9c) and the ports of this computer forwarded to boards.
// The settings are part of the project.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "model/schematic.hpp"
#endif

#include <QDialog>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace simnodus::ui {

class WifiDialog : public QDialog {
    Q_OBJECT
public:
    WifiDialog(const model::WifiSettings& settings, QWidget* parent = nullptr);

    // Valid after accept().
    model::WifiSettings settings() const { return result_; }

private:
    void add_forward(const model::WifiForwardSetting& forward);
    void validate_and_accept();
    void sync();

    model::WifiSettings result_;
    QCheckBox* access_point_ = nullptr;
    QGroupBox* network_ = nullptr;
    QLineEdit* ssid_ = nullptr;
    QComboBox* security_ = nullptr;
    QLineEdit* passphrase_ = nullptr;
    QSpinBox* channel_ = nullptr;
    QCheckBox* gateway_ = nullptr;
    QGroupBox* forwards_box_ = nullptr;
    QTableWidget* forwards_ = nullptr;
    QLabel* problem_ = nullptr;
};

} // namespace simnodus::ui
