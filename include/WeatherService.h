#pragma once

#include <stdint.h>
#include <WiFiS3.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "WeatherData.h"

// Static pool allocator for ArduinoJson v7 (NASA Power of Ten Rule 3: Zero heap allocation)
class StaticPoolAllocator : public ArduinoJson::Allocator {
public:
    static constexpr size_t POOL_SIZE = 4096;

    void* allocate(size_t size) override {
        size_t aligned = (size + 3) & ~static_cast<size_t>(3);
        if (m_used + aligned > POOL_SIZE) {
            return nullptr;
        }
        void* ptr = &m_pool[m_used];
        m_lastPtr = ptr;
        m_lastOffset = m_used;
        m_lastSize = aligned;
        m_used += aligned;
        return ptr;
    }

    void deallocate(void* /*ptr*/) override {
        // Pool allocator resets en masse, no per-slot free
    }

    void* reallocate(void* ptr, size_t new_size) override {
        size_t aligned = (new_size + 3) & ~static_cast<size_t>(3);
        if (ptr != nullptr && ptr == m_lastPtr) {
            if (m_lastOffset + aligned <= POOL_SIZE) {
                m_used = m_lastOffset + aligned;
                m_lastSize = aligned;
                return ptr;
            }
            return nullptr;
        }
        void* newPtr = allocate(aligned);
        if (newPtr != nullptr && ptr != nullptr) {
            size_t copyLen = (m_lastSize < aligned) ? m_lastSize : aligned;
            memcpy(newPtr, ptr, copyLen);
        }
        return newPtr;
    }

    void reset() {
        m_used = 0;
        m_lastPtr = nullptr;
        m_lastOffset = 0;
        m_lastSize = 0;
    }

private:
    uint8_t m_pool[POOL_SIZE] = {0};
    size_t  m_used = 0;
    void*   m_lastPtr = nullptr;
    size_t  m_lastOffset = 0;
    size_t  m_lastSize = 0;
};

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
    char                m_payloadBuffer[MAX_RESPONSE_BYTES]; // Fixed static response buffer
    StaticPoolAllocator m_allocator;                         // Static JSON memory pool

    // Bounded HTTP execution helpers (<= 60 lines each)
    bool executeHttpRequest(const char* host, uint16_t port, const char* path, bool useSsl);
    bool sendGetRequest(WiFiClient& client, const char* host, const char* path);
    bool skipToBody(WiFiClient& client, uint32_t startMs, uint32_t timeoutMs);
    bool readHttpResponse(WiFiClient& client, uint32_t timeoutMs);
    bool parsePayload(WeatherData& outData);
    bool parseSlots(JsonArrayConst slots, WeatherData& outData);
    bool parseFallback(const JsonDocument& doc, WeatherData& outData);
};
