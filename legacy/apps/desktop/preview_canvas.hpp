// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/symbol_preview.hpp"
#include <QWidget>

class PreviewCanvas final : public QWidget {
public:
    PreviewCanvas();
    void setCapture(std::shared_ptr<const simnodus::SymbolPreviewCapture> capture);
    const auto& capture() const { return capture_; }
    QRectF fittedView() const;
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    std::shared_ptr<const simnodus::SymbolPreviewCapture> capture_;
};
