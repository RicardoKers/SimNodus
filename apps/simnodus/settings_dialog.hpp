// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Simulation settings (SIM-01, SIM-03): interactive or transient mode and
// their time steps. The settings are part of the project.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "model/schematic.hpp"
#endif

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;

namespace simnodus::ui {

class SimulationSettingsDialog : public QDialog {
    Q_OBJECT
public:
    SimulationSettingsDialog(const model::SimulationSettings& settings, QWidget* parent = nullptr);

    // Valid after accept().
    model::SimulationSettings settings() const { return result_; }

private:
    void validate_and_accept();

    model::SimulationSettings result_;
    QRadioButton* interactive_ = nullptr;
    QRadioButton* transient_ = nullptr;
    QComboBox* interactive_step_ = nullptr;
    QLineEdit* segment_ = nullptr;
    QLineEdit* stop_ = nullptr;
    QLineEdit* transient_step_ = nullptr;
    QLabel* problem_ = nullptr;
};

} // namespace simnodus::ui
