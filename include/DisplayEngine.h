#pragma once

#include <stdint.h>
#include <Arduino_LED_Matrix.h>
#include "WeatherData.h"

// DisplayEngine encapsulates the physical 12x8 LED Matrix on Arduino UNO R4 WiFi.
// Complies with NASA Power of Ten rules: zero dynamic memory allocation, bounded loops.
class DisplayEngine {
public:
    DisplayEngine();

    // Initialize the physical LED matrix peripheral
    void init();

    // Render a predefined weather icon (Sun, Cloud, Rain, Thunder, Boot, Error)
    void renderIcon(uint8_t iconId);

    // Render 2-digit temperature in Celsius (-40C to 99C)
    void renderTemperature(int8_t tempC);

    // Render boot/startup logo
    void renderBoot();

    // Render error alert icon
    void renderError();

    // Clear all LEDs
    void clear();

private:
    ArduinoLEDMatrix m_matrix;
    uint8_t m_frame[8][12]; // 8 rows x 12 cols pixel buffer

    // Internal helper functions
    void clearBuffer();
    void commitFrame();
    void drawDigit(uint8_t digit, uint8_t startCol, uint8_t startRow);
    void drawMinus(uint8_t startCol, uint8_t startRow);
    void drawUnitC(uint8_t startCol, uint8_t startRow);
};
