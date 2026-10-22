#include "WeatherService.h"
#include <ArduinoJson.h>

WeatherService::WeatherService() {
    memset(m_payloadBuffer, 0, sizeof(m_payloadBuffer));
}

bool WeatherService::ensureWifiConnected(const char* ssid, const char* pass, uint32_t timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    uint8_t retries = 0;
    while (retries < MAX_WIFI_RETRIES) {
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(ssid);

        WiFi.begin(ssid, pass);
        uint32_t startMs = millis();

        while (WiFi.status() != WL_CONNECTED && (millis() - startMs < timeoutMs)) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("Wi-Fi connected! IP address: ");
            Serial.println(WiFi.localIP());
            return true;
        }

        retries++;
        Serial.print("Wi-Fi retry attempt ");
        Serial.print(retries);
        Serial.print(" of ");
        Serial.println(MAX_WIFI_RETRIES);
        delay(1000);
    }

    return false;
}

int32_t WeatherService::getRssi() const {
    return WiFi.RSSI();
}

bool WeatherService::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

bool WeatherService::readHttpResponse(WiFiClient& client, uint32_t timeoutMs) {
    uint32_t startMs = millis();

    while (!client.available()) {
        if (millis() - startMs >= timeoutMs) {
            Serial.println("HTTP response timeout.");
            return false;
        }
        delay(10);
    }

    // Skip HTTP headers: scan for double CRLF or '{'
    bool foundBody = false;
    uint16_t skipBytes = 0;
    constexpr uint16_t MAX_HEADER_SCAN = 1024;

    while (client.available() && skipBytes < MAX_HEADER_SCAN) {
        char c = (char)client.read();
        skipBytes++;
        if (c == '{') {
            m_payloadBuffer[0] = '{';
            foundBody = true;
            break;
        }
    }

    if (!foundBody) {
        return false;
    }

    // Read remaining body into static buffer
    uint16_t bufIndex = 1;
    uint16_t maxBuf = sizeof(m_payloadBuffer) - 1;
    while (client.available() && (bufIndex < maxBuf)) {
        m_payloadBuffer[bufIndex++] = (char)client.read();
    }
    m_payloadBuffer[bufIndex] = '\0';

    return (bufIndex > 1);
}

bool WeatherService::executeHttpRequest(const char* host, uint16_t port, const char* path, bool useSsl) {
    bool success = false;

    if (useSsl) {
        WiFiSSLClient sslClient;
        if (!sslClient.connect(host, port)) {
            Serial.println("SSL connection failed.");
            return false;
        }
        sslClient.print("GET "); sslClient.print(path); sslClient.println(" HTTP/1.1");
        sslClient.print("Host: "); sslClient.println(host);
        sslClient.println("User-Agent: AmbiDesk-UNO-R4");
        sslClient.println("Connection: close");
        sslClient.println();
        success = readHttpResponse(sslClient, HTTP_TIMEOUT_MS);
        sslClient.stop();
    } else {
        WiFiClient client;
        if (!client.connect(host, port)) {
            Serial.println("TCP connection failed.");
            return false;
        }
        client.print("GET "); client.print(path); client.println(" HTTP/1.1");
        client.print("Host: "); client.println(host);
        client.println("User-Agent: AmbiDesk-UNO-R4");
        client.println("Connection: close");
        client.println();
        success = readHttpResponse(client, HTTP_TIMEOUT_MS);
        client.stop();
    }
    return success;
}

bool WeatherService::parsePayload(WeatherData& outData) {
    if (m_payloadBuffer[0] == '\0') {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, m_payloadBuffer);
    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }

    outData.reset();

    // Check if slots array format is present
    JsonArrayConst slots = doc["slots"].as<JsonArrayConst>();
    if (!slots.isNull() && slots.size() > 0) {
        uint8_t count = 0;
        for (JsonObjectConst s : slots) {
            if (count >= FORECAST_SLOTS_COUNT) break;
            const char* lbl = s["lbl"] | "";
            strncpy(outData.slots[count].label, lbl, sizeof(outData.slots[count].label) - 1);
            outData.slots[count].label[sizeof(outData.slots[count].label) - 1] = '\0';
            outData.slots[count].temp = s["t"] | 0;
            outData.slots[count].icon = s["ico"] | (uint8_t)ICON_CLOUDY;
            outData.slots[count].pop  = s["pop"] | 0;
            count++;
        }
        outData.slotCount = count;
        outData.syncLegacyFields();
        outData.isValid = (count > 0);
    } else if (doc["cur_t"].is<int8_t>() && doc["cur_icon"].is<uint8_t>()) {
        // Fallback for simple current weather schema
        outData.currentTemp = doc["cur_t"].as<int8_t>();
        outData.currentIcon = doc["cur_icon"].as<uint8_t>();
        outData.nextIcon    = doc["nxt_icon"] | outData.currentIcon;
        outData.pop         = doc["pop"] | 0;

        strncpy(outData.slots[0].label, "Now", sizeof(outData.slots[0].label) - 1);
        outData.slots[0].temp = outData.currentTemp;
        outData.slots[0].icon = outData.currentIcon;
        outData.slots[0].pop  = outData.pop;
        outData.slotCount   = 1;
        outData.isValid     = true;
    } else {
        Serial.println("Invalid payload schema.");
        return false;
    }

    return outData.isValid;
}

bool WeatherService::fetchWeather(const char* host, uint16_t port, const char* path, bool useSsl, WeatherData& outData) {
    if (host == nullptr || path == nullptr) {
        return false;
    }

    memset(m_payloadBuffer, 0, sizeof(m_payloadBuffer));
    bool httpOk = executeHttpRequest(host, port, path, useSsl);
    if (!httpOk) {
        return false;
    }

    return parsePayload(outData);
}
