#pragma once

#include <stdint.h>
#include "WeatherData.h"
#include "DisplayEngine.h"
#include "WeatherService.h"

// Explicit Finite State Machine states
enum AppState : uint8_t {
    STATE_BOOT       = 0, // Wi-Fi setup & splash display
    STATE_FETCH      = 1, // Polling edge aggregator
    STATE_DISP_ICON  = 2, // Slot A: Current weather icon (3s)
    STATE_DISP_TEMP  = 3, // Slot B: Outdoor temperature (2s)
    STATE_DISP_ALERT = 4, // Slot C: Rain outlook warning (3s, conditional)
    STATE_ERROR      = 5  // Network/parse error & backoff
};

class AppFSM {
public:
    AppFSM(DisplayEngine& display, WeatherService& weather);

    // Initialize FSM and hardware components
    void init();

    // Main non-blocking update tick driven by millis()
    void update(uint32_t currentMillis);

    // Get current active state (for logging/telemetry)
    AppState getState() const;

    // Get cached weather data
    const WeatherData& getData() const;

private:
    DisplayEngine&  m_display;
    WeatherService& m_weather;
    WeatherData     m_data;
    AppState        m_state;
    uint32_t        m_stateTimerMs;
    uint32_t        m_lastPollMs;

    // State transition handlers (<= 60 lines each)
    void handleBoot(uint32_t currentMillis);
    void handleFetch(uint32_t currentMillis);
    void handleDispIcon(uint32_t currentMillis);
    void handleDispTemp(uint32_t currentMillis);
    void handleDispAlert(uint32_t currentMillis);
    void handleError(uint32_t currentMillis);
};
