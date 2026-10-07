#pragma once

#include <stdint.h>

// Standardized Weather Category IDs matching the Aggregator Data Dictionary
enum WeatherIconId : uint8_t {
    ICON_CLEAR       = 0, // Clear / Sunny
    ICON_CLOUDY      = 1, // Cloudy / Fog / Overcast
    ICON_RAIN        = 2, // Rain / Drizzle / Showers
    ICON_THUNDER     = 3, // Thunderstorm / Severe
    ICON_BOOT        = 4, // AmbiDesk boot / loading glyph
    ICON_ERROR       = 5  // Connection / parsing error glyph
};

// Weather data payload container
struct WeatherData {
    int8_t   currentTemp; // Outdoor temperature in Celsius (-40 to 60)
    uint8_t  currentIcon; // Weather category ID (WeatherIconId)
    uint8_t  nextIcon;    // Forecast category ID for +1 hour
    uint8_t  pop;         // Probability of precipitation (0 to 100)
    bool     isValid;     // Data validity flag

    void reset() {
        currentTemp = 0;
        currentIcon = ICON_BOOT;
        nextIcon    = ICON_BOOT;
        pop         = 0;
        isValid     = false;
    }
};
