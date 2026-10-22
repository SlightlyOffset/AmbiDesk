#include "WeatherService.h"
#include <Arduino.h>
#include <assert.h>
#include <string.h>

WeatherService::WeatherService() {
    memset(m_payloadBuffer, 0, sizeof(m_payloadBuffer));
    assert(m_payloadBuffer[0] == '\0');
    assert(sizeof(m_payloadBuffer) == MAX_RESPONSE_BYTES);
}

bool WeatherService::ensureWifiConnected(const char* ssid, const char* pass, uint32_t timeoutMs) {
    if (ssid == nullptr || pass == nullptr || timeoutMs == 0) {
        return false;
    }
    assert(ssid != nullptr && pass != nullptr);
    assert(timeoutMs > 0);

    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        return true;
    }

    uint8_t retries = 0;
    while (retries < MAX_WIFI_RETRIES) {
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(ssid);
        WiFi.begin(ssid, pass);

        uint32_t startMs = millis();
        uint16_t iteration = 0;
        constexpr uint16_t MAX_WIFI_ITERATIONS = 150; // 150 * 100ms = 15s max

        while ((WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) && 
               (millis() - startMs < timeoutMs) && 
               (iteration < MAX_WIFI_ITERATIONS)) {
            delay(100);
            iteration++;
            Serial.print(".");
        }
        assert(iteration <= MAX_WIFI_ITERATIONS);
        Serial.println();

        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            Serial.print("Wi-Fi connected! IP address: ");
            Serial.println(WiFi.localIP());
            return true;
        }

        retries++;
        Serial.print("Wi-Fi retry attempt ");
        Serial.print(retries);
        Serial.print(" of ");
        Serial.println(MAX_WIFI_RETRIES);
        delay(500);
    }
    assert(retries == MAX_WIFI_RETRIES);

    return false;
}

int32_t WeatherService::getRssi() const {
    if (!isConnected()) {
        return -100;
    }
    int32_t rssi = WiFi.RSSI();
    assert(rssi <= 0);
    return rssi;
}

bool WeatherService::isConnected() const {
    bool connected = (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0));
    assert(!connected || WiFi.status() == WL_CONNECTED);
    return connected;
}

bool WeatherService::skipToBody(WiFiClient& client, uint32_t startMs, uint32_t timeoutMs) {
    assert(timeoutMs > 0);
    uint16_t headerBytes = 0;
    uint16_t waitCycles = 0;
    uint8_t matchState = 0; // State machine matching \r\n\r\n (or \n\n)
    constexpr uint16_t MAX_HEADER_SCAN = 4096;
    constexpr uint16_t MAX_HEADER_WAIT_CYCLES = 1600; // 1600 * 5ms = 8000ms max

    while ((client.connected() || client.available()) && 
           (headerBytes < MAX_HEADER_SCAN) && 
           (waitCycles < MAX_HEADER_WAIT_CYCLES) && 
           (millis() - startMs < timeoutMs)) {
        if (!client.available()) {
            delay(5);
            waitCycles++;
            continue;
        }
        char c = static_cast<char>(client.read());
        headerBytes++;

        if ((matchState == 0 || matchState == 2) && c == '\r') {
            matchState++;
        } else if (c == '\n') {
            matchState = (matchState >= 2) ? 4 : 2;
            if (matchState == 4) {
                assert(headerBytes <= MAX_HEADER_SCAN);
                return true;
            }
        } else {
            matchState = 0;
        }
    }
    assert(headerBytes <= MAX_HEADER_SCAN);
    Serial.println("Failed to parse HTTP headers.");
    return false;
}

bool WeatherService::readHttpResponse(WiFiClient& client, uint32_t timeoutMs) {
    assert(timeoutMs > 0);
    uint32_t startMs = millis();
    uint16_t waitCycles = 0;
    constexpr uint16_t MAX_WAIT_CYCLES = 800; // 800 * 10ms = 8000ms max

    while (client.connected() && !client.available() && 
           (millis() - startMs < timeoutMs) && 
           (waitCycles < MAX_WAIT_CYCLES)) {
        delay(10);
        waitCycles++;
    }
    if (!client.available() || !skipToBody(client, startMs, timeoutMs)) {
        return false;
    }

    uint16_t bufIndex = 0;
    const uint16_t maxBuf = static_cast<uint16_t>(sizeof(m_payloadBuffer) - 1);
    uint16_t idleCycles = 0;
    uint8_t braceDepth = 0;
    bool foundJsonStart = false;
    constexpr uint16_t MAX_IDLE_CYCLES = 100; // 100 * 10ms = 1000ms max idle

    while ((client.connected() || client.available()) && 
           (bufIndex < maxBuf) && 
           (millis() - startMs < timeoutMs) && 
           (idleCycles < MAX_IDLE_CYCLES)) {
        if (!client.available()) {
            delay(10);
            idleCycles++;
            continue;
        }
        idleCycles = 0;
        char c = static_cast<char>(client.read());
        if (!foundJsonStart) {
            if (c == '{') {
                foundJsonStart = true;
                braceDepth = 1;
                m_payloadBuffer[bufIndex++] = c;
            }
            continue;
        }
        m_payloadBuffer[bufIndex++] = c;
        if (c == '{' && braceDepth < 255) braceDepth++;
        else if (c == '}' && braceDepth > 0) {
            braceDepth--;
            if (braceDepth == 0) break; // Finished root JSON object
        }
    }
    m_payloadBuffer[bufIndex] = '\0';
    assert(bufIndex < sizeof(m_payloadBuffer));
    return (foundJsonStart && braceDepth == 0 && bufIndex > 1);
}

bool WeatherService::sendGetRequest(WiFiClient& client, const char* host, const char* path) {
    if (host == nullptr || path == nullptr) {
        return false;
    }
    assert(host != nullptr);
    assert(path != nullptr);

    client.print("GET "); client.print(path); client.println(" HTTP/1.1");
    client.print("Host: "); client.println(host);
    client.println("User-Agent: AmbiDesk-UNO-R4");
    client.println("Connection: close");
    client.println();

    return readHttpResponse(client, HTTP_TIMEOUT_MS);
}

bool WeatherService::executeHttpRequest(const char* host, uint16_t port, const char* path, bool useSsl) {
    if (host == nullptr || path == nullptr) {
        return false;
    }
    assert(host != nullptr);
    assert(path != nullptr);

    bool success = false;
    if (useSsl) {
        WiFiSSLClient sslClient;
        if (!sslClient.connect(host, port)) {
            sslClient.stop();
            Serial.println("SSL connection failed.");
            return false;
        }
        success = sendGetRequest(sslClient, host, path);
        sslClient.stop();
    } else {
        WiFiClient client;
        if (!client.connect(host, port)) {
            client.stop();
            Serial.println("TCP connection failed.");
            return false;
        }
        success = sendGetRequest(client, host, path);
        client.stop();
    }
    return success;
}

bool WeatherService::parseSlots(JsonArrayConst slots, WeatherData& outData) {
    assert(!slots.isNull());
    uint8_t count = 0;
    for (JsonObjectConst s : slots) {
        if (count >= FORECAST_SLOTS_COUNT) break;
        const char* lbl = s["lbl"] | "";
        strncpy(outData.slots[count].label, lbl, sizeof(outData.slots[count].label) - 1);
        outData.slots[count].label[sizeof(outData.slots[count].label) - 1] = '\0';

        int32_t t = s["t"] | 0;
        if (t < -40) t = -40;
        if (t > 60)  t = 60;
        outData.slots[count].temp = static_cast<int8_t>(t);

        uint32_t ico = s["ico"] | static_cast<uint32_t>(ICON_CLOUDY);
        if (ico > static_cast<uint32_t>(ICON_ERROR)) ico = ICON_CLOUDY;
        outData.slots[count].icon = static_cast<uint8_t>(ico);

        int32_t popVal = s["pop"] | 0;
        if (popVal < 0)   popVal = 0;
        if (popVal > 100) popVal = 100;
        outData.slots[count].pop = static_cast<uint8_t>(popVal);

        count++;
    }
    assert(count <= FORECAST_SLOTS_COUNT);
    outData.slotCount = count;
    outData.syncLegacyFields();
    outData.isValid = (count > 0);
    return outData.isValid;
}

bool WeatherService::parseFallback(const JsonDocument& doc, WeatherData& outData) {
    assert(!doc.isNull());
    int32_t t = doc["cur_t"] | 0;
    if (t < -40) t = -40;
    if (t > 60)  t = 60;
    outData.currentTemp = static_cast<int8_t>(t);

    uint32_t ico = doc["cur_icon"] | static_cast<uint32_t>(ICON_CLOUDY);
    if (ico > static_cast<uint32_t>(ICON_ERROR)) ico = ICON_CLOUDY;
    outData.currentIcon = static_cast<uint8_t>(ico);

    uint32_t nxtIco = doc["nxt_icon"] | static_cast<uint32_t>(outData.currentIcon);
    if (nxtIco > static_cast<uint32_t>(ICON_ERROR)) nxtIco = outData.currentIcon;
    outData.nextIcon = static_cast<uint8_t>(nxtIco);

    int32_t popVal = doc["pop"] | 0;
    if (popVal < 0)   popVal = 0;
    if (popVal > 100) popVal = 100;
    outData.pop = static_cast<uint8_t>(popVal);

    strncpy(outData.slots[0].label, "Now", sizeof(outData.slots[0].label) - 1);
    outData.slots[0].label[sizeof(outData.slots[0].label) - 1] = '\0';
    outData.slots[0].temp = outData.currentTemp;
    outData.slots[0].icon = outData.currentIcon;
    outData.slots[0].pop  = outData.pop;
    outData.slotCount   = 1;
    outData.isValid     = true;
    assert(outData.slotCount == 1 && outData.pop <= 100);
    return true;
}

bool WeatherService::parsePayload(WeatherData& outData) {
    if (m_payloadBuffer[0] == '\0') {
        return false;
    }
    assert(m_payloadBuffer[0] == '{');

    m_allocator.reset();
    JsonDocument doc(&m_allocator);
    DeserializationError error = deserializeJson(doc, m_payloadBuffer);
    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }

    outData.reset();
    assert(!outData.isValid && outData.slotCount == 0);
    JsonArrayConst slots = doc["slots"].as<JsonArrayConst>();
    if (!slots.isNull() && slots.size() > 0) {
        return parseSlots(slots, outData);
    } else if (doc["cur_t"].is<int>() && doc["cur_icon"].is<int>()) {
        return parseFallback(doc, outData);
    }

    Serial.println("Invalid payload schema.");
    return false;
}

bool WeatherService::fetchWeather(const char* host, uint16_t port, const char* path, bool useSsl, WeatherData& outData) {
    if (host == nullptr || path == nullptr) {
        return false;
    }
    assert(host != nullptr);
    assert(path != nullptr);

    memset(m_payloadBuffer, 0, sizeof(m_payloadBuffer));
    bool httpOk = executeHttpRequest(host, port, path, useSsl);
    if (!httpOk) {
        return false;
    }

    return parsePayload(outData);
}
