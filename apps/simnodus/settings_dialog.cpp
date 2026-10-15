// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "settings_dialog.hpp"

#include "model/engineering.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

namespace simnodus::ui {

SimulationSettingsDialog::SimulationSettingsDialog(const model::SimulationSettings& settings, QWidget* parent)
    : QDialog(parent), result_(settings)
{
    setWindowTitle(tr("Simulation Settings"));
    auto* layout = new QVBoxLayout(this);

    interactive_ = new QRadioButton(tr("Interactive: runs continuously and reacts to switches, buttons and "
                                       "potentiometers"));
    transient_ = new QRadioButton(tr("Transient: simulates a fixed time span once; the complete result opens in "
                                     "the Signal Analyzer"));
    (settings.mode == model::SimulationMode::Interactive ? interactive_ : transient_)->setChecked(true);
    layout->addWidget(interactive_);
    layout->addWidget(transient_);

    auto* interactive_box = new QGroupBox(tr("Interactive mode"));
    auto* interactive_form = new QFormLayout(interactive_box);
    interactive_step_ = new QComboBox;
    interactive_step_->addItem(tr("100 µs (default, real time on classroom PCs)"), 1e-4);
    interactive_step_->addItem(tr("10 µs (faster circuits, more CPU)"), 1e-5);
    interactive_step_->addItem(tr("1 ms (slow circuits)"), 1e-3);
    const int step_index = interactive_step_->findData(settings.max_step);
    if (step_index >= 0) {
        interactive_step_->setCurrentIndex(step_index);
    } else {
        interactive_step_->addItem(QString::fromStdString(model::format_engineering(settings.max_step) + "s"),
            settings.max_step);
        interactive_step_->setCurrentIndex(interactive_step_->count() - 1);
    }
    interactive_form->addRow(tr("Maximum time step"), interactive_step_);
    segment_ = new QLineEdit(QString::fromStdString(model::format_engineering(settings.segment)));
    segment_->setToolTip(tr("The engine restarts every segment to keep memory bounded (0.5 s to 2 s)."));
    interactive_form->addRow(tr("Segment length (s)"), segment_);
    quantum_ = new QComboBox;
    quantum_->setObjectName(QStringLiteral("mcuQuantum"));
    quantum_->addItem(tr("1 ms (default)"), 1e-3);
    quantum_->addItem(tr("0.1 ms (faster input response, slower run)"), 1e-4);
    quantum_->addItem(tr("10 ms (faster run, inputs up to 10 ms late)"), 1e-2);
    const int quantum_index = quantum_->findData(settings.mcu_quantum);
    if (quantum_index >= 0) {
        quantum_->setCurrentIndex(quantum_index);
    } else {
        quantum_->addItem(QString::fromStdString(model::format_engineering(settings.mcu_quantum) + "s"),
            settings.mcu_quantum);
        quantum_->setCurrentIndex(quantum_->count() - 1);
    }
    quantum_->setToolTip(tr("Microcontroller coupling quantum: the firmware runs at most this far ahead of the "
                            "circuit, so circuit voltages reach its inputs within about this time."));
    interactive_form->addRow(tr("Microcontroller quantum"), quantum_);
    layout->addWidget(interactive_box);

    auto* transient_box = new QGroupBox(tr("Transient mode"));
    auto* transient_form = new QFormLayout(transient_box);
    stop_ = new QLineEdit(QString::fromStdString(model::format_engineering(settings.transient_stop)));
    transient_form->addRow(tr("Stop time (s)"), stop_);
    transient_step_ = new QLineEdit(QString::fromStdString(model::format_engineering(settings.transient_max_step)));
    transient_form->addRow(tr("Maximum time step (s)"), transient_step_);
    layout->addWidget(transient_box);

    auto* note = new QLabel(tr("Both modes start with discharged capacitors and inductors without current."));
    note->setWordWrap(true);
    layout->addWidget(note);
    problem_ = new QLabel;
    problem_->setStyleSheet(QStringLiteral("color: #b42318;"));
    layout->addWidget(problem_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &SimulationSettingsDialog::validate_and_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto sync = [this, interactive_box, transient_box] {
        interactive_box->setEnabled(interactive_->isChecked());
        transient_box->setEnabled(transient_->isChecked());
    };
    connect(interactive_, &QRadioButton::toggled, this, sync);
    sync();
}

void SimulationSettingsDialog::validate_and_accept()
{
    auto read = [](QLineEdit* edit) { return model::parse_engineering(edit->text().toStdString(), "s"); };
    const auto segment = read(segment_);
    const auto stop = read(stop_);
    const auto step = read(transient_step_);
    if (!segment || *segment < 0.1 || *segment > 10) {
        problem_->setText(tr("The segment length must be between 0.1 s and 10 s."));
        return;
    }
    if (!stop || *stop <= 0 || *stop > 3600) {
        problem_->setText(tr("The stop time must be positive and at most one hour."));
        return;
    }
    if (!step || *step <= 0 || *step > *stop / 10) {
        problem_->setText(tr("The maximum step must be positive and at most a tenth of the stop time."));
        return;
    }
    if (*stop / *step > 5e7) {
        problem_->setText(tr("Too many time steps (%1); use a larger step or a shorter stop time.")
                              .arg(QString::fromStdString(model::format_engineering(*stop / *step))));
        return;
    }
    result_.mode = interactive_->isChecked() ? model::SimulationMode::Interactive : model::SimulationMode::Transient;
    result_.max_step = interactive_step_->currentData().toDouble();
    result_.segment = *segment;
    result_.mcu_quantum = quantum_->currentData().toDouble();
    result_.transient_stop = *stop;
    result_.transient_max_step = *step;
    accept();
}

} // namespace simnodus::ui
