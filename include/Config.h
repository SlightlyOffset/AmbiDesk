#pragma once

#include <stdint.h>

// If secrets.h exists, include it for Wi-Fi credentials
#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Wi-Fi Configuration Defaults (override in secrets.h if desired)
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// Micro-Aggregator Server Configuration
// For Cloudflare Worker over HTTPS or local mock over HTTP
#ifndef AGGREGATOR_HOST
#define AGGREGATOR_HOST "ambidesk-aggregator.workers.dev"
#endif

#ifndef AGGREGATOR_PORT
#define AGGREGATOR_PORT 443
#endif

#ifndef AGGREGATOR_PATH
#define AGGREGATOR_PATH "/"
#endif

#ifndef AGGREGATOR_USE_SSL
#define AGGREGATOR_USE_SSL true
#endif

// LCD 1602 / 2004 I2C Hardware Configuration
#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR 0x27 // Common I2C addresses: 0x27 (PCF8574T) or 0x3F (PCF8574AT)
#endif

#ifndef LCD_COLS
#define LCD_COLS 16
#endif

#ifndef LCD_ROWS
#define LCD_ROWS 2
#endif

// External Button Input Pin (2-pin push button to D2 and GND)
#ifndef BUTTON_PIN
#define BUTTON_PIN 2
#endif

// Button Debounce & Interaction Timers (milliseconds)
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50UL;
constexpr uint32_t USER_INSPECT_TIMEOUT_MS = 20000UL; // 20s idle returns back to 'Now'

// Application Timing Parameters (milliseconds)
constexpr uint32_t POLL_INTERVAL_MS =
    600000UL; // 10 minutes between weather updates
constexpr uint32_t SLOT_A_ICON_MS = 3000UL; // Current weather icon display time
constexpr uint32_t SLOT_B_TEMP_MS = 2000UL; // Temperature display time
constexpr uint32_t SLOT_C_ALERT_MS =
    3000UL; // Next-hour rain warning display time
constexpr uint32_t ERROR_RETRY_MS =
    30000UL; // Wait 30s before retrying failed fetch

// Threshold for displaying Slot C precipitation alert
constexpr uint8_t POP_ALERT_THRESHOLD = 50; // Display warning if PoP > 50%

// Safety & Bounded Execution Limits (NASA Power of Ten Rule 2)
constexpr uint32_t WIFI_TIMEOUT_MS =
    15000UL; // Max 15s to establish Wi-Fi connection
constexpr uint32_t HTTP_TIMEOUT_MS = 8000UL; // Max 8s socket timeout
constexpr uint16_t MAX_RESPONSE_BYTES =
    768; // Hard boundary for HTTP body stream
constexpr uint8_t MAX_WIFI_RETRIES =
    3; // Max Wi-Fi reconnect attempts per cycle
