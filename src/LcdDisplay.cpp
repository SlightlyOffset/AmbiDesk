#include "LcdDisplay.h"
#include <stdio.h>

LcdDisplay::LcdDisplay(uint8_t addr, uint8_t cols, uint8_t rows)
    : m_lcd(addr, cols, rows),
      m_cols(cols),
      m_rows(rows) {
}

void LcdDisplay::init() {
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
    m_lcd.clear();
}

const char* LcdDisplay::getWeatherName(uint8_t iconId) const {
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

    m_lcd.setCursor(0, row);
    uint8_t len = 0;
    while (text[len] != '\0' && len < m_cols) {
        m_lcd.write(static_cast<uint8_t>(text[len]));
        len++;
    }
    // Pad remaining columns with spaces to clear old text without flickering
    while (len < m_cols) {
        m_lcd.write(' ');
        len++;
    }
}

void LcdDisplay::showBoot(const char* status) {
    printLine(0, "AmbiDesk Weather");
    printLine(1, status != nullptr ? status : "Starting up...");
}

void LcdDisplay::showForecastSlot(const ForecastSlot& slot) {
    char line1[32];
    char line2[32];

    // Format temperature with custom degree glyph (\1) immediately after the number (e.g. " 31°C")
    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%2d\1C", slot.temp);

    // Line 1: Day/Time label on the left (replaces "Clear"), temperature right-aligned (16 cols total)
    // E.g. "Now 11:00   31°C" or "+3h 14:00   32°C" or "+1d 23/10   31°C"
    snprintf(line1, sizeof(line1), "%-10s %5s", slot.label, tempStr);

    // Line 2: Weather condition name on the left and PoP% on the right (16 cols total)
    // E.g. "Rainy    PoP:13%" or "Clear    PoP: 0%"
    snprintf(line2, sizeof(line2), "%-8s PoP:%2d%%", getWeatherName(slot.icon), slot.pop);

    printLine(0, line1);
    printLine(1, line2);
}

void LcdDisplay::showWeather(const WeatherData& data) {
    if (data.slotCount > 0) {
        showForecastSlot(data.slots[0]);
    } else {
        // Fallback for empty slots
        ForecastSlot fallbackSlot;
        snprintf(fallbackSlot.label, sizeof(fallbackSlot.label), "Now");
        fallbackSlot.temp = data.currentTemp;
        fallbackSlot.icon = data.currentIcon;
        fallbackSlot.pop  = data.pop;
        showForecastSlot(fallbackSlot);
    }
}

void LcdDisplay::showFetching() {
    printLine(1, "Updating data...");
}

void LcdDisplay::showError(const char* reason) {
    printLine(0, "System Alert!");
    printLine(1, reason != nullptr ? reason : "Retry in 30s...");
}
