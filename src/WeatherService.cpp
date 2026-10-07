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

    // Skip headers: search for "\r\n\r\n" or find start of JSON '{'
    uint16_t skipBytes = 0;
    const uint16_t MAX_HEADER_BYTES = 2048;
    bool foundBody = false;

    while (client.available() && skipBytes < MAX_HEADER_BYTES) {
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
