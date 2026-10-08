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
    512; // Hard boundary for HTTP body stream
constexpr uint8_t MAX_WIFI_RETRIES =
    3; // Max Wi-Fi reconnect attempts per cycle
