// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "fuses_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace simnodus::ui {

namespace {

// The ATmega8's factory bytes (datasheet, Fuse Bits).
constexpr std::uint8_t factory_low = 0xE1;
constexpr std::uint8_t factory_high = 0xD9;
constexpr std::uint8_t factory_lock = 0xFF;

// Clock source groups of CKSEL3:0 (ATmega8A datasheet, Clock Sources): the
// value written for the group; crystals use CKSEL3:1, CKSEL0 belongs to the
// start-up time.
enum Group { ExternalClock, InternalRc, ExternalRc, LowFrequencyCrystal, Crystal };

Group group_of(int cksel)
{
    if (cksel == 0) {
        return ExternalClock;
    }
    if (cksel <= 4) {
        return InternalRc;
    }
    if (cksel <= 8) {
        return ExternalRc;
    }
    return cksel == 9 ? LowFrequencyCrystal : Crystal;
}

} // namespace

bool FusesDialog::parse_byte(const QString& text, std::uint8_t& value)
{
    QString t = text.trimmed();
    if (t.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        t = t.mid(2);
    }
    bool ok = false;
    const uint v = t.toUInt(&ok, 16);
    if (!ok || t.isEmpty() || t.size() > 2 || v > 0xFF) {
        return false;
    }
    value = static_cast<std::uint8_t>(v);
    return true;
}

FusesDialog::FusesDialog(const std::string& chip, std::uint8_t low, std::uint8_t high, std::uint8_t lock, QWidget* parent)
    : QDialog(parent), bytes_{low, high, lock}
{
    setWindowTitle(tr("Fuses of the %1").arg(chip == "atmega8" ? QStringLiteral("ATmega8") : QStringLiteral("ATmega8A")));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Programmed bits are 0, as on the chip; a checked box is a programmed bit. The fuses "
                                "take effect when the simulation starts, as when the chip is powered up."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    // The three bytes in hex.
    auto* bytes = new QGroupBox(tr("Bytes"));
    auto* bytes_form = new QFormLayout(bytes);
    const QString names[3] = {tr("Low fuse"), tr("High fuse"), tr("Lock bits")};
    for (int i = 0; i < 3; ++i) {
        auto* edit = new QLineEdit;
        edit->setObjectName(QStringLiteral("fuseByte%1").arg(i));
        edit->setMaxLength(4);
        edit->setInputMask(QString());
        connect(edit, &QLineEdit::textEdited, this, [this, i, edit](const QString& text) {
            std::uint8_t v = 0;
            if (parse_byte(text, v)) {
                bytes_[static_cast<std::size_t>(i)] = v;
                edit->setStyleSheet(QString());
                refresh();
            } else {
                edit->setStyleSheet(QStringLiteral("color: #c0392b;"));
            }
        });
        connect(edit, &QLineEdit::editingFinished, this, [this, i, edit] {
            edit->setText(QStringLiteral("%1").arg(bytes_[static_cast<std::size_t>(i)], 2, 16, QLatin1Char('0')).toUpper());
            edit->setStyleSheet(QString());
        });
        bytes_form->addRow(names[i], edit);
        hex_[static_cast<std::size_t>(i)] = edit;
    }
    layout->addWidget(bytes);

    auto* fields = new QGroupBox(tr("Fields"));
    form_ = new QFormLayout(fields);
    layout->addWidget(fields);

    // Low fuse: the clock source (CKSEL3:0) and its start-up time (SUT1:0,
    // and CKSEL0 for crystals).
    clock_ = new QComboBox;
    clock_->setObjectName(QStringLiteral("fuseClock"));
    const std::vector<std::pair<int, QString>> clocks = {
        {0x1, tr("Internal RC oscillator, 1 MHz (factory)")},
        {0x2, tr("Internal RC oscillator, 2 MHz")},
        {0x3, tr("Internal RC oscillator, 4 MHz")},
        {0x4, tr("Internal RC oscillator, 8 MHz")},
        {0xE, tr("Crystal or resonator, 3-8 MHz (1-16 MHz with CKOPT)")},
        {0xC, tr("Crystal or resonator, 0.9-3 MHz")},
        {0xA, tr("Ceramic resonator, 0.4-0.9 MHz")},
        {0x9, tr("Low-frequency crystal, 32.768 kHz")},
        {0x0, tr("External clock on XTAL1")},
        {0x5, tr("External RC oscillator, up to 0.9 MHz")},
        {0x6, tr("External RC oscillator, 0.9-3 MHz")},
        {0x7, tr("External RC oscillator, 3-8 MHz")},
        {0x8, tr("External RC oscillator, 8-12 MHz")},
    };
    for (const auto& [value, text] : clocks) {
        clock_->addItem(text, value);
    }
    start_up_ = new QComboBox;
    start_up_->setObjectName(QStringLiteral("fuseStartUp"));
    form_->addRow(tr("Clock source"), clock_);
    form_->addRow(tr("Start-up time"), start_up_);
    connect(clock_, &QComboBox::activated, this, [this] {
        const int cksel = clock_->currentData().toInt();
        const int keep_sut = (bytes_[0] >> 4) & 3;
        bytes_[0] = static_cast<std::uint8_t>((bytes_[0] & 0xC0) | (keep_sut << 4) | cksel);
        if (group_of(cksel) == Crystal) {
            bytes_[0] = static_cast<std::uint8_t>((bytes_[0] & 0xC0) | 0x20 | (cksel | 1)); // 16K CK + 4.1 ms
        } else if (group_of(cksel) == LowFrequencyCrystal && keep_sut == 3) {
            bytes_[0] = static_cast<std::uint8_t>((bytes_[0] & 0xCF) | 0x10);
        }
        refresh();
    });
    connect(start_up_, &QComboBox::activated, this, [this] {
        const int code = start_up_->currentData().toInt(); // SUT, plus CKSEL0 in bit 2 for crystals
        const int cksel = bytes_[0] & 0x0F;
        if (group_of(cksel) == Crystal) {
            bytes_[0] = static_cast<std::uint8_t>((bytes_[0] & 0xC0) | ((code & 3) << 4) | (cksel & 0x0E) | ((code >> 2) & 1));
        } else {
            bytes_[0] = static_cast<std::uint8_t>((bytes_[0] & 0xCF) | ((code & 3) << 4));
        }
        refresh();
    });
    add_bit(0, 6, tr("Brown-out detection (BODEN)"), tr("Resets the chip while VCC is below the brown-out level"));
    add_bit(0, 7, tr("Brown-out level 4.0 V (BODLEVEL; 2.7 V when not programmed)"));

    // High fuse.
    add_bit(1, 4, tr("CKOPT: full-swing crystal oscillator"), tr("Needed for crystals above 8 MHz"));
    add_bit(1, 7, tr("RSTDISBL: PC6 is an I/O pin instead of RESET"),
        tr("On a real chip this also disables programming through the ISP connector"));
    add_bit(1, 6, tr("WDTON: watchdog always on"));
    add_bit(1, 3, tr("EESAVE: the EEPROM is kept on a chip erase"));
    add_bit(1, 5, tr("SPIEN: programming through the ISP connector (no effect in SimNodus)"));
    add_field(1, 0x06, tr("Boot section (BOOTSZ)"),
        {{0x06, tr("128 words, from 0x0F80")}, {0x04, tr("256 words, from 0x0F00")}, {0x02, tr("512 words, from 0x0E00")},
            {0x00, tr("1024 words, from 0x0C00 (factory)")}});
    add_bit(1, 0, tr("BOOTRST: the reset vector is the boot section's start"));

    // Lock bits.
    add_field(2, 0x03, tr("Memory lock (LB)"),
        {{0x03, tr("No lock (factory)")}, {0x02, tr("No further programming (no effect in SimNodus)")},
            {0x00, tr("No programming or verification (no effect in SimNodus)")}});
    add_field(2, 0x0C, tr("Application section (BLB0)"),
        {{0x0C, tr("No restriction")}, {0x08, tr("SPM may not write it")}, {0x00, tr("SPM may not write it, LPM from the boot section may not read it")},
            {0x04, tr("LPM from the boot section may not read it")}});
    add_field(2, 0x30, tr("Boot section (BLB1)"),
        {{0x30, tr("No restriction")}, {0x20, tr("SPM may not write it")}, {0x00, tr("SPM may not write it, LPM from the application may not read it")},
            {0x10, tr("LPM from the application may not read it")}});

    summary_ = new QLabel;
    summary_->setObjectName(QStringLiteral("fuseSummary"));
    summary_->setWordWrap(true);
    layout->addWidget(summary_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Factory values"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        bytes_ = {factory_low, factory_high, factory_lock};
        refresh();
    });
    layout->addWidget(buttons);
    refresh();
}

void FusesDialog::add_bit(int byte, int bit, const QString& text, const QString& tip)
{
    auto* box = new QCheckBox(text);
    box->setObjectName(QStringLiteral("fuseBit%1_%2").arg(byte).arg(bit));
    box->setToolTip(tip);
    connect(box, &QCheckBox::toggled, this, [this, byte, bit](bool programmed) {
        if (updating_) {
            return;
        }
        auto& b = bytes_[static_cast<std::size_t>(byte)];
        b = static_cast<std::uint8_t>(programmed ? (b & ~(1u << bit)) : (b | (1u << bit)));
        refresh();
    });
    form_->addRow(QString(), box);
    bits_.push_back({byte, bit, box});
}

FusesDialog::Field& FusesDialog::add_field(
    int byte, std::uint8_t mask, const QString& label, const std::vector<std::pair<int, QString>>& values)
{
    auto* box = new QComboBox;
    box->setObjectName(QStringLiteral("fuseField%1_%2").arg(byte).arg(mask));
    for (const auto& [value, text] : values) {
        box->addItem(text, value);
    }
    connect(box, &QComboBox::activated, this, [this, byte, mask, box] {
        auto& b = bytes_[static_cast<std::size_t>(byte)];
        b = static_cast<std::uint8_t>((b & ~mask) | (box->currentData().toInt() & mask));
        refresh();
    });
    form_->addRow(label, box);
    fields_.push_back({byte, mask, box});
    return fields_.back();
}

void FusesDialog::start_up_choices()
{
    const int cksel = bytes_[0] & 0x0F;
    start_up_->clear();
    switch (group_of(cksel)) {
    case ExternalClock:
    case InternalRc:
        start_up_->addItem(tr("6 CK, no delay (with brown-out detection)"), 0);
        start_up_->addItem(tr("6 CK + 4.1 ms (fast rising power)"), 1);
        start_up_->addItem(tr("6 CK + 65 ms (slowly rising power; factory)"), 2);
        break;
    case ExternalRc:
        start_up_->addItem(tr("18 CK, no delay (with brown-out detection)"), 0);
        start_up_->addItem(tr("18 CK + 4.1 ms"), 1);
        start_up_->addItem(tr("18 CK + 65 ms"), 2);
        start_up_->addItem(tr("6 CK + 4.1 ms"), 3);
        break;
    case LowFrequencyCrystal:
        start_up_->addItem(tr("1K CK + 4.1 ms"), 0);
        start_up_->addItem(tr("1K CK + 65 ms"), 1);
        start_up_->addItem(tr("32K CK + 65 ms (stable frequency at start-up)"), 2);
        break;
    case Crystal:
        start_up_->addItem(tr("258 CK + 4.1 ms (ceramic resonator, fast rising power)"), 0);
        start_up_->addItem(tr("258 CK + 65 ms (ceramic resonator, slowly rising power)"), 1);
        start_up_->addItem(tr("1K CK, no delay (ceramic resonator, brown-out detection)"), 2);
        start_up_->addItem(tr("1K CK + 4.1 ms (ceramic resonator, fast rising power)"), 3);
        start_up_->addItem(tr("1K CK + 65 ms (ceramic resonator, slowly rising power)"), 4);
        start_up_->addItem(tr("16K CK, no delay (crystal, brown-out detection)"), 5);
        start_up_->addItem(tr("16K CK + 4.1 ms (crystal, fast rising power)"), 6);
        start_up_->addItem(tr("16K CK + 65 ms (crystal, slowly rising power)"), 7);
        break;
    }
}

void FusesDialog::refresh()
{
    updating_ = true;
    for (int i = 0; i < 3; ++i) {
        auto* edit = hex_[static_cast<std::size_t>(i)];
        const QString text = QStringLiteral("%1").arg(bytes_[static_cast<std::size_t>(i)], 2, 16, QLatin1Char('0')).toUpper();
        std::uint8_t shown = 0;
        if (!parse_byte(edit->text(), shown) || shown != bytes_[static_cast<std::size_t>(i)]) {
            edit->setText(text);
        }
    }
    const int cksel = bytes_[0] & 0x0F;
    const int sut = (bytes_[0] >> 4) & 3;
    const Group group = group_of(cksel);
    const int clock_value = group == Crystal ? (cksel & 0x0E) : cksel;
    clock_->setCurrentIndex(clock_->findData(clock_value));
    start_up_choices();
    const int code = group == Crystal ? (((cksel & 1) << 2) | sut) : sut;
    start_up_->setCurrentIndex(start_up_->findData(code)); // -1: a reserved combination
    for (const auto& b : bits_) {
        b.box->setChecked(((bytes_[static_cast<std::size_t>(b.byte)] >> b.bit) & 1u) == 0);
    }
    for (const auto& f : fields_) {
        f.box->setCurrentIndex(f.box->findData(bytes_[static_cast<std::size_t>(f.byte)] & f.mask));
    }
    summary_->setText(clock_summary());
    updating_ = false;
}

QString FusesDialog::clock_summary() const
{
    const int cksel = bytes_[0] & 0x0F;
    switch (group_of(cksel)) {
    case InternalRc:
        return tr("The chip runs on its internal RC oscillator at %1 MHz; XTAL1 and XTAL2 are I/O pins (PB6, PB7).")
            .arg(1 << (cksel - 1));
    case Crystal:
    case LowFrequencyCrystal:
        return tr("The chip needs a Crystal part between XTAL1 (PB6) and XTAL2 (PB7); without one it does not start.");
    case ExternalClock:
        return tr("The chip needs a clock on XTAL1 (PB6); SimNodus does not read one from the circuit yet, so it does not "
                  "start.");
    case ExternalRc:
        return tr("The chip needs an RC network on XTAL1 (PB6); SimNodus does not simulate one yet, so it does not start.");
    }
    return QString();
}

} // namespace simnodus::ui
