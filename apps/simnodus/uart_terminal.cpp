// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "uart_terminal.hpp"

#include "run_controller.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace simnodus::ui {

namespace {

constexpr int max_transcript = 200000; // characters kept per port

// Appends received bytes: CR LF and lone CR end a line, other control
// characters except tab show as their code. With `times`, every line starts
// with the virtual time of its first character.
void append(QString& text, bool& after_cr, bool& line_start, const std::string& bytes,
    const std::vector<std::uint64_t>* times)
{
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c == '\n') {
            if (!after_cr) {
                text += QLatin1Char('\n');
                line_start = true;
            }
            after_cr = false;
            continue;
        }
        after_cr = c == '\r';
        if (c == '\r') {
            text += QLatin1Char('\n');
            line_start = true;
            continue;
        }
        if (line_start && times && i < times->size()) {
            text += QStringLiteral("[%1 s] ").arg(static_cast<double>((*times)[i]) / 1e9, 0, 'f', 6);
        }
        line_start = false;
        if (c < 0x20 && c != '\t') {
            text += QStringLiteral("<%1>").arg(c, 2, 16, QLatin1Char('0'));
        } else {
            text += QChar(c); // Latin-1
        }
    }
    if (text.size() > max_transcript) {
        text.remove(0, text.size() - max_transcript);
    }
}

} // namespace

UartTerminal::UartTerminal(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Port")));
    ports_ = new QComboBox;
    ports_->setObjectName(QStringLiteral("uartPort"));
    ports_->setMinimumContentsLength(28);
    top->addWidget(ports_, 1);
    stamps_ = new QCheckBox(tr("Time stamps"));
    stamps_->setObjectName(QStringLiteral("uartTimestamps"));
    stamps_->setToolTip(tr("Start each received line with its simulated time"));
    top->addWidget(stamps_);
    auto* clear = new QPushButton(tr("Clear"));
    top->addWidget(clear);
    layout->addLayout(top);

    output_ = new QPlainTextEdit;
    output_->setObjectName(QStringLiteral("uartOutput"));
    output_->setReadOnly(true);
    output_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    output_->setPlaceholderText(tr("Run a circuit with a microcontroller; what its UART transmits appears here."));
    layout->addWidget(output_, 1);

    auto* bottom = new QHBoxLayout;
    input_ = new QLineEdit;
    input_->setObjectName(QStringLiteral("uartInput"));
    input_->setPlaceholderText(tr("Text to send"));
    bottom->addWidget(input_, 1);
    ending_ = new QComboBox;
    ending_->setObjectName(QStringLiteral("uartEnding"));
    ending_->addItem(tr("No line ending"), QString());
    ending_->addItem(tr("CR"), QStringLiteral("\r"));
    ending_->addItem(tr("LF"), QStringLiteral("\n"));
    ending_->addItem(tr("CR+LF"), QStringLiteral("\r\n"));
    bottom->addWidget(ending_);
    send_ = new QPushButton(tr("Send"));
    send_->setObjectName(QStringLiteral("uartSend"));
    bottom->addWidget(send_);
    layout->addLayout(bottom);

    connect(send_, &QPushButton::clicked, this, &UartTerminal::send);
    connect(input_, &QLineEdit::returnPressed, this, &UartTerminal::send);
    connect(ports_, &QComboBox::currentIndexChanged, this, [this](int) { show_current(); });
    connect(clear, &QPushButton::clicked, this, [this] {
        transcripts_[ports_->currentIndex()].text.clear();
        output_->clear();
    });
    reload();
}

void UartTerminal::set_controller(RunController* controller)
{
    controller_ = controller;
    reload();
}

void UartTerminal::reload()
{
    const QString previous = ports_->currentText();
    ports_->blockSignals(true);
    ports_->clear();
    transcripts_.clear();
    if (controller_) {
        for (const auto& port : controller_->uart_ports()) {
            ports_->addItem(port.title);
        }
    }
    const int index = ports_->findText(previous);
    ports_->setCurrentIndex(index >= 0 ? index : 0);
    ports_->blockSignals(false);
    const bool any = ports_->count() > 0;
    ports_->setEnabled(any);
    input_->setEnabled(any);
    send_->setEnabled(any);
    output_->clear();
}

void UartTerminal::refresh()
{
    if (!controller_) {
        return;
    }
    const auto ports = controller_->uart_ports();
    if (static_cast<int>(ports.size()) != ports_->count()) {
        reload();
    }
    std::string bytes;
    std::vector<std::uint64_t> times;
    for (int i = 0; i < static_cast<int>(ports.size()); ++i) {
        auto& t = transcripts_[i];
        t.next = controller_->uart_received(ports[static_cast<std::size_t>(i)], t.next, bytes, &times);
        if (bytes.empty()) {
            continue;
        }
        if (i == ports_->currentIndex()) {
            // Append without rebuilding the whole document; keep the view at the end.
            QString added;
            append(added, t.after_cr, t.line_start, bytes, stamps_->isChecked() ? &times : nullptr);
            t.text += added;
            const bool at_end = output_->verticalScrollBar()->value() == output_->verticalScrollBar()->maximum();
            output_->moveCursor(QTextCursor::End);
            output_->insertPlainText(added);
            if (at_end) {
                output_->verticalScrollBar()->setValue(output_->verticalScrollBar()->maximum());
            }
            if (output_->document()->characterCount() > max_transcript * 2) {
                show_current();
            }
        } else {
            append(t.text, t.after_cr, t.line_start, bytes, stamps_->isChecked() ? &times : nullptr);
        }
    }
}

void UartTerminal::show_current()
{
    output_->setPlainText(transcripts_[ports_->currentIndex()].text);
    output_->moveCursor(QTextCursor::End);
}

void UartTerminal::send()
{
    if (!controller_ || ports_->currentIndex() < 0) {
        return;
    }
    const auto ports = controller_->uart_ports();
    if (ports_->currentIndex() >= static_cast<int>(ports.size())) {
        return;
    }
    const QString text = input_->text() + ending_->currentData().toString();
    if (text.isEmpty()) {
        return;
    }
    const QByteArray bytes = text.toLatin1();
    controller_->uart_send(ports[static_cast<std::size_t>(ports_->currentIndex())], bytes.toStdString());
    input_->clear();
}

} // namespace simnodus::ui
