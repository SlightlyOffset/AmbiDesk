#pragma once

#include <stdint.h>
#include <assert.h>

// Standardized Weather Category IDs matching the Aggregator Data Dictionary
enum WeatherIconId : uint8_t {
    ICON_CLEAR       = 0, // Clear / Sunny
    ICON_CLOUDY      = 1, // Cloudy / Fog / Overcast
    ICON_RAIN        = 2, // Rain / Drizzle / Showers
    ICON_THUNDER     = 3, // Thunderstorm / Severe
    ICON_BOOT        = 4, // AmbiDesk boot / loading glyph
    ICON_ERROR       = 5  // Connection / parsing error glyph
};

// 9 forecast timeline slots:
// 0: Now, 1: +1h, 2: +3h, 3: +5h, 4: +12h, 5: +1d, 6: +2d, 7: +3d, 8: +4d
constexpr uint8_t FORECAST_SLOTS_COUNT = 9;

struct ForecastSlot {
    char    label[12]; // e.g. "Now 11:00", "+3h 14:00", "+1d 23/10"
    int8_t  temp;      // Outdoor temperature in Celsius (-40 to 60)
    uint8_t icon;      // Weather category ID (WeatherIconId)
    uint8_t pop;       // Probability of precipitation (0 to 100)
};

// Weather data payload container
struct WeatherData {
    // Current weather legacy fields
    int8_t   currentTemp; // Outdoor temperature in Celsius (-40 to 60)
    uint8_t  currentIcon; // Weather category ID (WeatherIconId)
    uint8_t  nextIcon;    // Forecast category ID for +1 hour
    uint8_t  pop;         // Probability of precipitation for +1 hour (0 to 100)
    bool     isValid;     // Data validity flag

    // 9-step timeline slots
    ForecastSlot slots[FORECAST_SLOTS_COUNT];
    uint8_t      slotCount;

    void reset() {
        currentTemp = 0;
        currentIcon = ICON_BOOT;
        nextIcon    = ICON_BOOT;
        pop         = 0;
        isValid     = false;
        slotCount   = 0;

        for (uint8_t i = 0; i < FORECAST_SLOTS_COUNT; ++i) {
            slots[i].label[0] = '\0';
            slots[i].temp = 0;
            slots[i].icon = ICON_BOOT;
            slots[i].pop  = 0;
        }
    }

    void syncLegacyFields() {
        if (slotCount > 0) {
            currentTemp = slots[0].temp;
            currentIcon = slots[0].icon;
            // Sync pop with +1h slot (slots[1]) to match nxt_icon for rain alert display in Slot C
            pop         = (slotCount > 1) ? slots[1].pop : slots[0].pop;
            nextIcon    = (slotCount > 1) ? slots[1].icon : slots[0].icon;
        }
    }

    const ForecastSlot* getSlot(uint8_t index) const {
        if (index < slotCount && index < FORECAST_SLOTS_COUNT) {
            return &slots[index];
        }
        return nullptr;
    }
};
