#include "WeatherService.h"
#include "Config.h"
#include <Arduino.h>
#include <WiFiS3.h>
#include <ArduinoJson.h>
#include <string.h>

WeatherService::WeatherService() {
    memset(m_payloadBuffer, 0, sizeof(m_payloadBuffer));
}

bool WeatherService::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

int32_t WeatherService::getRssi() const {
    if (WiFi.status() != WL_CONNECTED) {
        return -100;
    }
    return WiFi.RSSI();
}

bool WeatherService::ensureWifiConnected(const char* ssid, const char* pass, uint32_t timeoutMs) {
    if (ssid == nullptr || pass == nullptr || timeoutMs == 0) {
        return false;
    }

    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);

    uint32_t startMs = millis();
    uint16_t iteration = 0;
    const uint16_t MAX_ITERATIONS = 300;

    while (WiFi.status() != WL_CONNECTED && 
           (millis() - startMs < timeoutMs) && 
           (iteration < MAX_ITERATIONS)) {
        delay(100);
        iteration++;
    }

    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        Serial.print("Wi-Fi connected! IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Wi-Fi connection timed out.");
    }
    return connected;
}

bool WeatherService::readHttpResponse(WiFiClient& client, uint32_t timeoutMs) {
    uint32_t startMs = millis();
    uint16_t waitCycles = 0;
    const uint16_t MAX_WAIT_CYCLES = 500;

    while (client.connected() && !client.available() && 
           (millis() - startMs < timeoutMs) && 
           (waitCycles < MAX_WAIT_CYCLES)) {
        delay(10);
        waitCycles++;
    }

    if (!client.available()) {
        return false;
    }

    // 1. Skip HTTP headers: match end-of-header sequence "\r\n\r\n"
    uint16_t headerBytes = 0;
    const uint16_t MAX_HEADER_BYTES = 4096;
    uint8_t matchState = 0; // State machine matching \r \n \r \n

    while ((client.connected() || client.available()) && 
           (millis() - startMs < timeoutMs) && 
           (headerBytes < MAX_HEADER_BYTES)) {
        if (!client.available()) {
            delay(5);
            continue;
        }

        char c = (char)client.read();
        headerBytes++;

        if (matchState == 0 && c == '\r') matchState = 1;
        else if (matchState == 1 && c == '\n') matchState = 2;
        else if (matchState == 2 && c == '\r') matchState = 3;
        else if (matchState == 3 && c == '\n') {
            matchState = 4; // Found \r\n\r\n
            break;
        } else if (c == '\n') {
            if (matchState == 2) {
                matchState = 4;
                break;
            }
            matchState = 2;
        } else if (c == '\r') {
            matchState = 1;
        } else {
            matchState = 0;
        }
    }

    if (matchState != 4) {
        Serial.println("Failed to parse HTTP headers.");
        return false;
    }

    // 2. Read JSON body: ignore any leading whitespace until '{' and read until '}'
    uint16_t bufIndex = 0;
    const uint16_t maxBuf = sizeof(m_payloadBuffer) - 1;
    bool foundJsonStart = false;

    while ((client.connected() || client.available()) && 
           (millis() - startMs < timeoutMs) && 
           (bufIndex < maxBuf)) {
        if (!client.available()) {
            delay(5);
            continue;
        }

        char c = (char)client.read();

        if (!foundJsonStart) {
            if (c == '{') {
                foundJsonStart = true;
                m_payloadBuffer[bufIndex++] = c;
            }
            continue;
        }

        m_payloadBuffer[bufIndex++] = c;
        if (c == '}') {
            break; // Finished reading root JSON object
        }
    }

    m_payloadBuffer[bufIndex] = '\0';
    Serial.print("Received payload: ");
    Serial.println(m_payloadBuffer);

    return (foundJsonStart && bufIndex > 1);
}

bool WeatherService::executeHttpRequest(const char* host, uint16_t port, const char* path, bool useSsl) {
    if (host == nullptr || path == nullptr) {
        return false;
    }

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

    if (!doc["cur_t"].is<int8_t>() || !doc["cur_icon"].is<uint8_t>()) {
        Serial.println("Invalid payload schema.");
        return false;
    }

    outData.currentTemp = doc["cur_t"].as<int8_t>();
    outData.currentIcon = doc["cur_icon"].as<uint8_t>();
    outData.nextIcon    = doc["nxt_icon"] | outData.currentIcon;
    outData.pop         = doc["pop"] | 0;
    outData.isValid     = true;

    return true;
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
