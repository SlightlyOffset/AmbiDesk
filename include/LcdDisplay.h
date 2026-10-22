#pragma once

#include <stdint.h>
#include <LiquidCrystal_I2C.h>
#include "WeatherData.h"
#include "Config.h"

// LcdDisplay encapsulates HD44780 LCD via I2C PCF8574 backpack (1602 / 2004).
// Adheres to NASA Power of Ten rules: zero heap allocation, bounded execution.
class LcdDisplay {
public:
    static constexpr uint8_t CUSTOM_CHAR_DEGREE = 1; // Custom CGRAM slot for degree symbol (°)

    explicit LcdDisplay(uint8_t addr = LCD_I2C_ADDR, uint8_t cols = LCD_COLS, uint8_t rows = LCD_ROWS);

    // Initialize I2C LCD peripheral and backlight
    void init();

    // Display boot splash or initialization progress
    void showBoot(const char* status = nullptr);

    // Display full weather summary (defaults to slot 0: Now)
    void showWeather(const WeatherData& data);

    // Display a specific forecast slot (Day/Time label replaces condition on line 1)
    void showForecastSlot(const ForecastSlot& slot);

    // Display background update notification
    void showFetching();

    // Display error alert with message
    void showError(const char* reason = nullptr);

    // Clear display
    void clear();

private:
    LiquidCrystal_I2C m_lcd;
    uint8_t           m_cols;
    uint8_t           m_rows;

    const char* getWeatherName(uint8_t iconId) const;
    void printLine(uint8_t row, const char* text);
};
