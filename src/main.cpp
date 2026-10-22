#include <Arduino.h>
#include "Config.h"
#include "DisplayEngine.h"
#include "LcdDisplay.h"
#include "WeatherService.h"
#include "AppFSM.h"

// Static instance allocations (NASA Power of Ten Rule 3: Zero heap allocation)
static DisplayEngine  g_display;
static LcdDisplay     g_lcd;
static WeatherService g_weather;
static AppFSM         g_fsm(g_display, g_lcd, g_weather);

void setup() {
    Serial.begin(115200);
    
    // Give USB serial a brief window to attach if monitoring
    uint32_t startMs = millis();
    while (!Serial && (millis() - startMs < 2000)) {
        // Non-blocking wait for CDC serial
    }

    Serial.println("========================================");
    Serial.println("   AmbiDesk Phase 1 MVP Initializing    ");
    Serial.println("========================================");

    g_fsm.init();
}

void loop() {
    // Non-blocking cooperative scheduler tick
    g_fsm.update(millis());
}
