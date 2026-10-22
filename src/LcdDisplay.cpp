#include "LcdDisplay.h"
#include <stdio.h>
#include <assert.h>

LcdDisplay::LcdDisplay(uint8_t addr, uint8_t cols, uint8_t rows)
    : m_lcd(addr, cols > MAX_LCD_COLS ? MAX_LCD_COLS : cols, rows > MAX_LCD_ROWS ? MAX_LCD_ROWS : rows),
      m_cols(cols > MAX_LCD_COLS ? MAX_LCD_COLS : cols),
      m_rows(rows > MAX_LCD_ROWS ? MAX_LCD_ROWS : rows) {
    assert(m_cols <= MAX_LCD_COLS);
    assert(m_rows <= MAX_LCD_ROWS);
}

void LcdDisplay::init() {
    assert(m_cols > 0 && m_cols <= MAX_LCD_COLS);
    assert(m_rows > 0 && m_rows <= MAX_LCD_ROWS);
    m_lcd.init();
    m_lcd.backlight();

    // 5x8 custom glyph for degree symbol (°)
    uint8_t degreeChar[8] = {
        0b00110, //  .##..
        0b01001, //  #..#.
        0b01001, //  #..#.
        0b00110, //  .##..
        0b00000,
        0b00000,
        0b00000,
        0b00000
    };
    m_lcd.createChar(CUSTOM_CHAR_DEGREE, degreeChar);
    m_lcd.clear();
}

void LcdDisplay::clear() {
    assert(m_cols <= MAX_LCD_COLS);
    assert(m_rows <= MAX_LCD_ROWS);
    m_lcd.clear();
}

const char* LcdDisplay::getWeatherName(uint8_t iconId) const {
    assert(iconId <= ICON_ERROR || iconId > ICON_ERROR);
    switch (iconId) {
        case ICON_CLEAR:   return "Clear";
        case ICON_CLOUDY:  return "Cloudy";
        case ICON_RAIN:    return "Rainy";
        case ICON_THUNDER: return "Thunder";
        default:           return "Weather";
    }
}

void LcdDisplay::printLine(uint8_t row, const char* text) {
    if (row >= m_rows || text == nullptr) return;

    const uint8_t maxCols = (m_cols > MAX_LCD_COLS) ? MAX_LCD_COLS : m_cols;
    assert(row < MAX_LCD_ROWS);
    assert(maxCols <= MAX_LCD_COLS);
    m_lcd.setCursor(0, row);
    uint8_t len = 0;
    while (text[len] != '\0' && len < maxCols) {
        m_lcd.write(static_cast<uint8_t>(text[len]));
        len++;
    }
    // Pad remaining columns with spaces to clear old text without flickering
    while (len < maxCols) {
        m_lcd.write(' ');
        len++;
    }
    assert(len == maxCols);
}

void LcdDisplay::showBoot(const char* status) {
    const char* line2 = (status != nullptr) ? status : "Starting up...";
    assert(line2 != nullptr);
    assert(m_rows >= 2);
    printLine(0, "AmbiDesk Weather");
    printLine(1, line2);
}

void LcdDisplay::showForecastSlot(const ForecastSlot& slot) {
    char line1[32];
    char line2[32];

    // Clamp temperature [-40, 60] and pop [0, 100]
    int8_t clampedTemp = slot.temp;
    if (clampedTemp < -40) clampedTemp = -40;
    if (clampedTemp > 60)  clampedTemp = 60;
    unsigned int clampedPop = (slot.pop > 100) ? 100 : slot.pop;
    assert(clampedTemp >= -40 && clampedTemp <= 60);
    assert(clampedPop <= 100);

    // Format temperature with custom degree glyph (\1) (e.g. " 31°C" or "-10°C")
    char tempStr[8];
    (void)snprintf(tempStr, sizeof(tempStr), "%2d\1C", clampedTemp);

    // Line 1: Label up to 10 chars, temp 5 chars -> exactly 16 cols
    (void)snprintf(line1, sizeof(line1), "%-10.10s %5s", slot.label, tempStr);

    // Line 2: Condition 7 chars, PoP up to 3 digits + % -> exactly 16 cols
    (void)snprintf(line2, sizeof(line2), "%-7.7s PoP:%3u%%", getWeatherName(slot.icon), clampedPop);

    printLine(0, line1);
    printLine(1, line2);
}

void LcdDisplay::showWeather(const WeatherData& data) {
    assert(data.slotCount <= FORECAST_SLOTS_COUNT);
    if (data.slotCount > 0) {
        assert(data.slots[0].pop <= 100);
        showForecastSlot(data.slots[0]);
    } else {
        ForecastSlot fallbackSlot;
        (void)snprintf(fallbackSlot.label, sizeof(fallbackSlot.label), "Now");
        fallbackSlot.temp = data.currentTemp;
        fallbackSlot.icon = data.currentIcon;
        fallbackSlot.pop  = data.pop;
        assert(fallbackSlot.label[0] == 'N');
        showForecastSlot(fallbackSlot);
    }
}

void LcdDisplay::showFetching() {
    assert(m_cols <= MAX_LCD_COLS);
    assert(m_rows >= 2);
    printLine(0, "AmbiDesk Weather");
    printLine(1, "Updating data...");
}

void LcdDisplay::showError(const char* reason) {
    const char* msg = (reason != nullptr) ? reason : "Retry in 30s...";
    assert(msg != nullptr);
    assert(m_rows >= 2);
    printLine(0, "System Alert!");
    printLine(1, msg);
}
