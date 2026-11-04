// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Fuses... dialog of an AVR chip (experiment E-10, SN-258): the low fuse, high
// fuse and lock bytes in hex, and the same bits decoded field by field (the
// clock source and its start-up time, brown-out, the boot section, the lock
// modes), both kept in step. Programmed bits are 0, as on the chip; a checked
// box is a programmed bit. The fields are described per chip in this
// dialog's code, so that their texts are translated with the interface.
#pragma once

#include <QDialog>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QFormLayout;

namespace simnodus::ui {

class FusesDialog : public QDialog {
    Q_OBJECT

public:
    // `chip`: "atmega8", "atmega8a", "atmega328" or "atmega328p"; the bytes as
    // the part holds them (the extended fuse: the 328/328P's).
    FusesDialog(const std::string& chip, std::uint8_t low, std::uint8_t high, std::uint8_t lock, QWidget* parent = nullptr,
        std::uint8_t extended = 0xFF);

    std::uint8_t low() const { return bytes_[0]; }
    std::uint8_t high() const { return bytes_[1]; }
    std::uint8_t lock() const { return bytes_[2]; }
    std::uint8_t extended() const { return bytes_[3]; }
    bool has_extended() const { return m328_; }
    // A chip's factory bytes: low, high, lock, extended.
    static std::array<std::uint8_t, 4> factory(const std::string& chip);
    // A sentence for the chosen clock, as the dialog shows it.
    QString clock_summary() const;

    // Parses "E1", "0xE1" or "e1"; false for anything else.
    static bool parse_byte(const QString& text, std::uint8_t& value);

private:
    struct Bit {
        int byte; // 0 low, 1 high, 2 lock, 3 extended
        int bit;
        QCheckBox* box;
    };
    struct Field {
        int byte;
        std::uint8_t mask;
        QComboBox* box; // item data: the field's bits in place
    };
    void refresh(); // the editors from bytes_
    void start_up_choices();
    void add_bit(int byte, int bit, const QString& text, const QString& tip = {});
    Field& add_field(int byte, std::uint8_t mask, const QString& label, const std::vector<std::pair<int, QString>>& values);

    std::array<std::uint8_t, 4> bytes_{};
    std::array<QLineEdit*, 4> hex_{};
    bool m328_ = false;
    bool m32_ = false;
    int group(int cksel) const;
    std::vector<Bit> bits_;
    std::vector<Field> fields_;
    QComboBox* clock_ = nullptr;
    QComboBox* start_up_ = nullptr;
    QLabel* summary_ = nullptr;
    QFormLayout* form_ = nullptr;
    bool updating_ = false;
};

} // namespace simnodus::ui
