#include "DisplayEngine.h"
#include "Glyphs.h"
#include <string.h>

DisplayEngine::DisplayEngine() {
    clearBuffer();
}

void DisplayEngine::init() {
    m_matrix.begin();
    clear();
}

void DisplayEngine::clearBuffer() {
    memset(m_frame, 0, sizeof(m_frame));
}

void DisplayEngine::commitFrame() {
    m_matrix.loadPixels(&m_frame[0][0], 96);
}

void DisplayEngine::clear() {
    clearBuffer();
    m_matrix.clear();
}

void DisplayEngine::renderIcon(uint8_t iconId) {
    clearBuffer();
    const uint8_t (*selectedIcon)[12] = Glyphs::BITMAP_CLOUD;

    if (iconId == ICON_CLEAR) {
        selectedIcon = Glyphs::BITMAP_SUN;
    } else if (iconId == ICON_CLOUDY) {
        selectedIcon = Glyphs::BITMAP_CLOUD;
    } else if (iconId == ICON_RAIN) {
        selectedIcon = Glyphs::BITMAP_RAIN;
    } else if (iconId == ICON_THUNDER) {
        selectedIcon = Glyphs::BITMAP_THUNDER;
    } else if (iconId == ICON_BOOT) {
        selectedIcon = Glyphs::BITMAP_BOOT;
    } else {
        selectedIcon = Glyphs::BITMAP_ERROR;
    }

    // Bounded copy (8 rows x 12 cols = 96 elements)
    for (uint8_t r = 0; r < 8; ++r) {
        for (uint8_t c = 0; c < 12; ++c) {
            m_frame[r][c] = selectedIcon[r][c];
        }
    }
    commitFrame();
}

void DisplayEngine::drawDigit(uint8_t digit, uint8_t startCol, uint8_t startRow) {
    if (digit > 9 || startCol > 9 || startRow > 3) {
        return; // Guard against boundary overflow
    }

    for (uint8_t r = 0; r < 5; ++r) {
        uint8_t rowBits = Glyphs::FONT_3X5[digit][r];
        for (uint8_t c = 0; c < 3; ++c) {
            bool pixelOn = (rowBits & (1 << (2 - c))) != 0;
            m_frame[startRow + r][startCol + c] = pixelOn ? 1 : 0;
        }
    }
}

void DisplayEngine::drawMinus(uint8_t startCol, uint8_t startRow) {
    if (startCol > 9 || startRow > 5) {
        return;
    }
    m_frame[startRow + 2][startCol + 0] = 1;
    m_frame[startRow + 2][startCol + 1] = 1;
    m_frame[startRow + 2][startCol + 2] = 1;
}

void DisplayEngine::drawUnitC(uint8_t startCol, uint8_t startRow) {
    if (startCol > 10 || startRow > 3) {
        return;
    }
    // Compact degree circle dot at row 0, col 10
    m_frame[startRow][startCol] = 1;
    // 'C' glyph: 2 cols wide, 4 rows high
    m_frame[startRow + 1][startCol] = 1;
    m_frame[startRow + 1][startCol + 1] = 1;
    m_frame[startRow + 2][startCol] = 1;
    m_frame[startRow + 3][startCol] = 1;
    m_frame[startRow + 4][startCol] = 1;
    m_frame[startRow + 4][startCol + 1] = 1;
}

void DisplayEngine::renderTemperature(int8_t tempC) {
    clearBuffer();

    int8_t clampedTemp = tempC;
    if (clampedTemp > 99) clampedTemp = 99;
    if (clampedTemp < -40) clampedTemp = -40;

    const uint8_t baseRow = 1;

    if (clampedTemp < 0) {
        drawMinus(0, baseRow);
        uint8_t absTemp = (uint8_t)(-clampedTemp);
        uint8_t tens = absTemp / 10;
        uint8_t ones = absTemp % 10;
        if (tens > 0) {
            drawDigit(tens, 3, baseRow);
            drawDigit(ones, 7, baseRow);
        } else {
            drawDigit(ones, 4, baseRow);
            drawUnitC(9, baseRow);
        }
    } else {
        uint8_t tens = (uint8_t)clampedTemp / 10;
        uint8_t ones = (uint8_t)clampedTemp % 10;
        drawDigit(tens, 1, baseRow);
        drawDigit(ones, 5, baseRow);
        drawUnitC(9, baseRow);
    }

    commitFrame();
}

void DisplayEngine::renderBoot() {
    renderIcon(ICON_BOOT);
}

void DisplayEngine::renderError() {
    renderIcon(ICON_ERROR);
}
