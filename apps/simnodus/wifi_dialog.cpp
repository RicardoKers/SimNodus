// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "wifi_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace simnodus::ui {

WifiDialog::WifiDialog(const model::WifiSettings& settings, QWidget* parent) : QDialog(parent), result_(settings)
{
    setWindowTitle(tr("Wi-Fi Network"));
    setObjectName(QStringLiteral("wifiDialog"));
    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("The ESP32-C3 boards of this project see this network. Firmware written for it "
                                "connects with the name and password below (experimental)."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    access_point_ = new QCheckBox(tr("SimNodus's access point"));
    access_point_->setObjectName(QStringLiteral("wifiAccessPoint"));
    access_point_->setChecked(settings.access_point);
    layout->addWidget(access_point_);

    network_ = new QGroupBox(tr("Network"));
    auto* form = new QFormLayout(network_);
    ssid_ = new QLineEdit(QString::fromStdString(settings.ssid));
    ssid_->setObjectName(QStringLiteral("wifiSsid"));
    ssid_->setMaxLength(32);
    form->addRow(tr("Name (SSID)"), ssid_);
    security_ = new QComboBox;
    security_->setObjectName(QStringLiteral("wifiSecurity"));
    security_->addItem(tr("WPA2-PSK"), true);
    security_->addItem(tr("Open (no password)"), false);
    security_->setCurrentIndex(settings.wpa2 ? 0 : 1);
    form->addRow(tr("Security"), security_);
    passphrase_ = new QLineEdit(QString::fromStdString(settings.passphrase));
    passphrase_->setObjectName(QStringLiteral("wifiPassphrase"));
    passphrase_->setMaxLength(63);
    passphrase_->setToolTip(tr("8 to 63 characters. The network exists only in the simulation."));
    form->addRow(tr("Password"), passphrase_);
    channel_ = new QSpinBox;
    channel_->setObjectName(QStringLiteral("wifiChannel"));
    channel_->setRange(1, 13);
    channel_->setValue(settings.channel);
    form->addRow(tr("Channel"), channel_);
    auto* addresses = new QLabel(tr("Boards get 192.168.4.2 and up by DHCP; the access point is 192.168.4.1."));
    addresses->setWordWrap(true);
    form->addRow(addresses);
    layout->addWidget(network_);

    gateway_ = new QCheckBox(tr("Gateway to this computer's network"));
    gateway_->setObjectName(QStringLiteral("wifiGateway"));
    gateway_->setChecked(settings.gateway);
    layout->addWidget(gateway_);
    auto* warning = new QLabel(tr("With the gateway, the boards' firmware reaches this computer (as host.simnodus), "
                                  "the local network and the internet, as a device on this computer's network would. "
                                  "Turn it on only for firmware you trust. When this project is run on another "
                                  "computer, SimNodus asks there first."));
    warning->setWordWrap(true);
    layout->addWidget(warning);

    forwards_box_ = new QGroupBox(tr("Ports of this computer forwarded to boards"));
    auto* forwards_layout = new QVBoxLayout(forwards_box_);
    forwards_ = new QTableWidget(0, 3);
    forwards_->setObjectName(QStringLiteral("wifiForwards"));
    forwards_->setHorizontalHeaderLabels({tr("Port on 127.0.0.1"), tr("Board address"), tr("Board port")});
    forwards_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    forwards_->verticalHeader()->hide();
    forwards_layout->addWidget(forwards_);
    auto* buttons_row = new QHBoxLayout;
    auto* add = new QPushButton(tr("Add"));
    auto* remove = new QPushButton(tr("Remove"));
    buttons_row->addWidget(add);
    buttons_row->addWidget(remove);
    buttons_row->addStretch();
    forwards_layout->addLayout(buttons_row);
    auto* forwards_note = new QLabel(tr("A browser on this computer opens a board's web server at "
                                        "http://127.0.0.1:<port>/ while the simulation runs."));
    forwards_note->setWordWrap(true);
    forwards_layout->addWidget(forwards_note);
    layout->addWidget(forwards_box_);
    for (const auto& forward : settings.forwards) {
        add_forward(forward);
    }
    connect(add, &QPushButton::clicked, this, [this] {
        model::WifiForwardSetting forward;
        forward.host_port = 8080 + forwards_->rowCount();
        add_forward(forward);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (forwards_->currentRow() >= 0) {
            forwards_->removeRow(forwards_->currentRow());
        }
    });

    problem_ = new QLabel;
    problem_->setStyleSheet(QStringLiteral("color: #b42318;"));
    problem_->setWordWrap(true);
    layout->addWidget(problem_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &WifiDialog::validate_and_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(access_point_, &QCheckBox::toggled, this, &WifiDialog::sync);
    connect(gateway_, &QCheckBox::toggled, this, &WifiDialog::sync);
    connect(security_, &QComboBox::currentIndexChanged, this, &WifiDialog::sync);
    sync();
}

void WifiDialog::add_forward(const model::WifiForwardSetting& forward)
{
    const int row = forwards_->rowCount();
    forwards_->insertRow(row);
    forwards_->setItem(row, 0, new QTableWidgetItem(QString::number(forward.host_port)));
    forwards_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(forward.station)));
    forwards_->setItem(row, 2, new QTableWidgetItem(QString::number(forward.station_port)));
}

void WifiDialog::sync()
{
    const bool on = access_point_->isChecked();
    network_->setEnabled(on);
    passphrase_->setEnabled(on && security_->currentData().toBool());
    gateway_->setEnabled(on);
    forwards_box_->setEnabled(on && gateway_->isChecked());
}

void WifiDialog::validate_and_accept()
{
    const std::string ssid = ssid_->text().toStdString();
    const std::string passphrase = passphrase_->text().toStdString();
    const bool wpa2 = security_->currentData().toBool();
    if (ssid.empty() || ssid.size() > 32) {
        problem_->setText(tr("The network's name has 1 to 32 characters."));
        return;
    }
    if (wpa2 && (passphrase.size() < 8 || passphrase.size() > 63)) {
        problem_->setText(tr("A WPA2 password has 8 to 63 characters."));
        return;
    }
    std::vector<model::WifiForwardSetting> forwards;
    for (int row = 0; row < forwards_->rowCount(); ++row) {
        const auto text = [this, row](int column) {
            const auto* item = forwards_->item(row, column);
            return item ? item->text().trimmed() : QString();
        };
        bool host_ok = false;
        bool port_ok = false;
        model::WifiForwardSetting forward;
        forward.host_port = text(0).toInt(&host_ok);
        forward.station = text(1).toStdString();
        forward.station_port = text(2).toInt(&port_ok);
        const QHostAddress address(text(1));
        if (!host_ok || !port_ok || forward.host_port < 1 || forward.host_port > 65535 || forward.station_port < 1
            || forward.station_port > 65535 || address.protocol() != QAbstractSocket::IPv4Protocol) {
            problem_->setText(tr("Row %1 of the forwarded ports needs a port of this computer, a board's address like "
                                 "192.168.4.2 and the board's port.")
                                  .arg(row + 1));
            return;
        }
        forwards.push_back(forward);
    }
    result_.access_point = access_point_->isChecked();
    result_.ssid = ssid;
    result_.wpa2 = wpa2;
    if (wpa2) {
        result_.passphrase = passphrase;
    }
    result_.channel = channel_->value();
    result_.gateway = gateway_->isChecked();
    result_.forwards = forwards;
    accept();
}

} // namespace simnodus::ui
