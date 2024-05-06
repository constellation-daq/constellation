#pragma once

namespace {

    static const int alpha = 96;
    static QColor level_colours[] = {
        QColor(224, 224, 224, alpha), // DEBUG
        QColor(192, 255, 192, alpha), // OK
        QColor(255, 224, 224, alpha), // THROW
        QColor(192, 208, 255, alpha), // EXTRA
        QColor(255, 255, 192, alpha), // INFO
        QColor(255, 224, 96, alpha),  // WARN
        QColor(255, 96, 96, alpha),   // ERROR
        QColor(208, 96, 255, alpha),  // USER
        QColor(192, 255, 192, alpha), // BUSY
        QColor(192, 192, 255, alpha), // NONE
    };
} // namespace
