#include "AppFSM.h"
#include "Config.h"
#include <Arduino.h>

AppFSM::AppFSM(DisplayEngine& display, WeatherService& weather)
    : m_display(display),
      m_weather(weather),
      m_state(STATE_BOOT),
      m_stateTimerMs(0),
      m_lastPollMs(0) {
    m_data.reset();
}

void AppFSM::init() {
    m_display.init();
    m_display.renderBoot();
    m_state = STATE_BOOT;
    m_stateTimerMs = millis();
    m_lastPollMs = 0;
    Serial.println("[FSM] Initialized. Entering STATE_BOOT.");
}

AppState AppFSM::getState() const {
    return m_state;
}

const WeatherData& AppFSM::getData() const {
    return m_data;
}

void AppFSM::handleBoot(uint32_t currentMillis) {
    Serial.println("[FSM] STATE_BOOT: Connecting to Wi-Fi...");
    bool wifiOk = m_weather.ensureWifiConnected(WIFI_SSID, WIFI_PASSWORD, WIFI_TIMEOUT_MS);
    if (wifiOk) {
        Serial.println("[FSM] Wi-Fi OK -> Transition to STATE_FETCH");
        m_state = STATE_FETCH;
        m_stateTimerMs = currentMillis;
    } else {
        Serial.println("[FSM] Wi-Fi Failed -> Transition to STATE_ERROR");
        m_display.renderError();
        m_state = STATE_ERROR;
        m_stateTimerMs = currentMillis;
    }
}

void AppFSM::handleFetch(uint32_t currentMillis) {
    Serial.println("[FSM] STATE_FETCH: Requesting weather data...");
    bool fetchOk = m_weather.fetchWeather(AGGREGATOR_HOST, AGGREGATOR_PORT, AGGREGATOR_PATH, AGGREGATOR_USE_SSL, m_data);

    if (fetchOk && m_data.isValid) {
        Serial.print("[FSM] Fetch Success: Temp=");
        Serial.print(m_data.currentTemp);
        Serial.print("C, Icon=");
        Serial.print(m_data.currentIcon);
        Serial.print(", NextIcon=");
        Serial.print(m_data.nextIcon);
        Serial.print(", PoP=");
        Serial.print(m_data.pop);
        Serial.println("%");

        m_lastPollMs = currentMillis;
        m_state = STATE_DISP_ICON;
        m_stateTimerMs = currentMillis;
        m_display.renderIcon(m_data.currentIcon);
    } else {
        Serial.println("[FSM] Fetch Failed -> Transition to STATE_ERROR");
        m_display.renderError();
        m_state = STATE_ERROR;
        m_stateTimerMs = currentMillis;
    }
}

void AppFSM::handleDispIcon(uint32_t currentMillis) {
    if (currentMillis - m_lastPollMs >= POLL_INTERVAL_MS) {
        m_state = STATE_FETCH;
        m_stateTimerMs = currentMillis;
        return;
    }

    if (currentMillis - m_stateTimerMs >= SLOT_A_ICON_MS) {
        Serial.print("[FSM] Slot A complete -> Slot B: Temp ");
        Serial.print(m_data.currentTemp);
        Serial.println("C");

        m_state = STATE_DISP_TEMP;
        m_stateTimerMs = currentMillis;
        m_display.renderTemperature(m_data.currentTemp);
    }
}

void AppFSM::handleDispTemp(uint32_t currentMillis) {
    if (currentMillis - m_lastPollMs >= POLL_INTERVAL_MS) {
        m_state = STATE_FETCH;
        m_stateTimerMs = currentMillis;
        return;
    }

    if (currentMillis - m_stateTimerMs >= SLOT_B_TEMP_MS) {
        if (m_data.pop > POP_ALERT_THRESHOLD) {
            Serial.print("[FSM] Slot B complete -> Slot C (Alert): Rain PoP ");
            Serial.print(m_data.pop);
            Serial.println("%");

            m_state = STATE_DISP_ALERT;
            m_stateTimerMs = currentMillis;
            m_display.renderPoP(m_data.pop);
        } else {
            Serial.print("[FSM] Slot B complete -> Slot A: Weather Icon ");
            Serial.print(m_data.currentIcon);
            Serial.print(" (PoP ");
            Serial.print(m_data.pop);
            Serial.println("% <= threshold)");

            m_state = STATE_DISP_ICON;
            m_stateTimerMs = currentMillis;
            m_display.renderIcon(m_data.currentIcon);
        }
    }
}

void AppFSM::handleDispAlert(uint32_t currentMillis) {
    if (currentMillis - m_lastPollMs >= POLL_INTERVAL_MS) {
        m_state = STATE_FETCH;
        m_stateTimerMs = currentMillis;
        return;
    }

    if (currentMillis - m_stateTimerMs >= SLOT_C_ALERT_MS) {
        Serial.print("[FSM] Slot C complete -> Slot A: Weather Icon ");
        Serial.println(m_data.currentIcon);

        m_state = STATE_DISP_ICON;
        m_stateTimerMs = currentMillis;
        m_display.renderIcon(m_data.currentIcon);
    }
}

void AppFSM::handleError(uint32_t currentMillis) {
    if (currentMillis - m_stateTimerMs >= ERROR_RETRY_MS) {
        Serial.println("[FSM] Error backoff elapsed. Retrying fetch...");
        if (!m_weather.isConnected()) {
            m_state = STATE_BOOT;
        } else {
            m_state = STATE_FETCH;
        }
        m_stateTimerMs = currentMillis;
    }
}

void AppFSM::update(uint32_t currentMillis) {
    switch (m_state) {
        case STATE_BOOT:
            handleBoot(currentMillis);
            break;
        case STATE_FETCH:
            handleFetch(currentMillis);
            break;
        case STATE_DISP_ICON:
            handleDispIcon(currentMillis);
            break;
        case STATE_DISP_TEMP:
            handleDispTemp(currentMillis);
            break;
        case STATE_DISP_ALERT:
            handleDispAlert(currentMillis);
            break;
        case STATE_ERROR:
            handleError(currentMillis);
            break;
        default:
            m_state = STATE_BOOT;
            m_stateTimerMs = currentMillis;
            break;
    }
}
