#pragma once

#include <stdint.h>
#include <WiFiS3.h>
#include "Config.h"
#include "WeatherData.h"

// WeatherService manages Wi-Fi state and HTTP/HTTPS communication with the micro-aggregator.
// Adheres to NASA Power of Ten: deterministic loop bounds, zero dynamic memory allocation.
class WeatherService {
public:
    WeatherService();

    // Ensure Wi-Fi connection is established within bounded timeout
    bool ensureWifiConnected(const char* ssid, const char* pass, uint32_t timeoutMs);

    // Fetch and parse weather data into outData struct
    bool fetchWeather(const char* host, uint16_t port, const char* path, bool useSsl, WeatherData& outData);

    // Get current Wi-Fi RSSI (signal strength)
    int32_t getRssi() const;

    // Check if Wi-Fi is actively connected
    bool isConnected() const;

private:
    char m_payloadBuffer[MAX_RESPONSE_BYTES]; // Fixed static response buffer

    // Bounded HTTP execution helpers (<= 60 lines each)
    bool executeHttpRequest(const char* host, uint16_t port, const char* path, bool useSsl);
    bool readHttpResponse(WiFiClient& client, uint32_t timeoutMs);
    bool parsePayload(WeatherData& outData);
};
