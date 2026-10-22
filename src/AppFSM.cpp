#include "AppFSM.h"
#include "Config.h"
#include <Arduino.h>

AppFSM::AppFSM(DisplayEngine& display, LcdDisplay& lcd, WeatherService& weather)
    : m_display(display),
      m_lcd(lcd),
      m_weather(weather),
      m_state(STATE_BOOT),
      m_stateTimerMs(0),
      m_lastPollMs(0),
      m_selectedSlot(0),
      m_inspectTimerMs(0),
      m_lastButtonReading(HIGH),
      m_debouncedButtonState(HIGH),
      m_lastDebounceMs(0) {
    m_data.reset();
}

void AppFSM::init() {
    // Configure button with internal pull-up resistor (Active LOW)
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    m_lastButtonReading = digitalRead(BUTTON_PIN);
    m_debouncedButtonState = m_lastButtonReading;
    m_lastDebounceMs = millis();

    m_display.init();
    m_display.renderBoot();
    m_lcd.init();
    m_lcd.showBoot("Connecting WiFi");
    m_state = STATE_BOOT;
    m_stateTimerMs = millis();
    m_lastPollMs = 0;
    m_selectedSlot = 0;
    Serial.println("[FSM] Initialized with Button on Pin 2. Entering STATE_BOOT.");
}

AppState AppFSM::getState() const {
    return m_state;
}

const WeatherData& AppFSM::getData() const {
    return m_data;
}

uint8_t AppFSM::getSelectedSlot() const {
    return m_selectedSlot;
}

void AppFSM::checkButton(uint32_t currentMillis) {
    uint8_t reading = digitalRead(BUTTON_PIN);

    // If signal changed, reset debounce timer
    if (reading != m_lastButtonReading) {
        m_lastDebounceMs = currentMillis;
        m_lastButtonReading = reading;
    }

    // When reading is stable for BUTTON_DEBOUNCE_MS
    if ((currentMillis - m_lastDebounceMs) >= BUTTON_DEBOUNCE_MS) {
        if (reading != m_debouncedButtonState) {
            m_debouncedButtonState = reading;
            // Button pressed down (Active LOW with INPUT_PULLUP)
            if (m_debouncedButtonState == LOW) {
                handleButtonPress(currentMillis);
            }
        }
    }
}

void AppFSM::handleButtonPress(uint32_t currentMillis) {
    if (!m_data.isValid || m_data.slotCount == 0) {
        Serial.println("[Button] Click detected, but weather data is not ready yet.");
        return;
    }

    // Step to the next forecast slot (0 to slotCount - 1)
    m_selectedSlot = (m_selectedSlot + 1) % m_data.slotCount;

    Serial.print("[Button] Click! Switched to Slot ");
    Serial.print(m_selectedSlot);
    Serial.print(" (");
    Serial.print(m_data.slots[m_selectedSlot].label);
    Serial.println(")");

    if (m_selectedSlot == 0) {
        // Cycled back to Present (Now)
        Serial.println("[Button] Cycled back to Present (Now)");
        m_state = STATE_DISP_ICON;
        m_stateTimerMs = currentMillis;
        m_lcd.showForecastSlot(m_data.slots[0]);
        m_display.renderIcon(m_data.slots[0].icon);
    } else {
        // Inspecting future forecast slot (+1h, +3h, +5h, +12h, +1d, +2d, +3d, +4d)
        m_state = STATE_INSPECT;
        m_inspectTimerMs = currentMillis;
        m_lcd.showForecastSlot(m_data.slots[m_selectedSlot]);
        m_display.renderIcon(m_data.slots[m_selectedSlot].icon);
    }
}

void AppFSM::handleBoot(uint32_t currentMillis) {
    Serial.println("[FSM] STATE_BOOT: Connecting to Wi-Fi...");
    m_lcd.showBoot("Connecting WiFi");
    bool wifiOk = m_weather.ensureWifiConnected(WIFI_SSID, WIFI_PASSWORD, WIFI_TIMEOUT_MS);
    if (wifiOk) {
        Serial.println("[FSM] Wi-Fi OK -> Transition to STATE_FETCH");
        m_lcd.showBoot("WiFi OK! Syncing");
        m_state = STATE_FETCH;
        m_stateTimerMs = currentMillis;
    } else {
        Serial.println("[FSM] Wi-Fi Failed -> Transition to STATE_ERROR");
        m_display.renderError();
        m_lcd.showError("WiFi Conn Failed");
        m_state = STATE_ERROR;
        m_stateTimerMs = currentMillis;
    }
}

void AppFSM::handleFetch(uint32_t currentMillis) {
    Serial.println("[FSM] STATE_FETCH: Requesting weather data...");
    m_lcd.showFetching();
    bool fetchOk = m_weather.fetchWeather(AGGREGATOR_HOST, AGGREGATOR_PORT, AGGREGATOR_PATH, AGGREGATOR_USE_SSL, m_data);

    if (fetchOk && m_data.isValid) {
        Serial.print("[FSM] Fetch Success: Slots=");
        Serial.print(m_data.slotCount);
        Serial.print(", Now Temp=");
        Serial.print(m_data.currentTemp);
        Serial.print("C, Icon=");
        Serial.print(m_data.currentIcon);
        Serial.print(", PoP=");
        Serial.print(m_data.pop);
        Serial.println("%");

        m_selectedSlot = 0;
        m_lastPollMs = currentMillis;
        m_state = STATE_DISP_ICON;
        m_stateTimerMs = currentMillis;
        m_display.renderIcon(m_data.currentIcon);
        m_lcd.showWeather(m_data);
    } else {
        Serial.println("[FSM] Fetch Failed -> Transition to STATE_ERROR");
        m_display.renderError();
        m_lcd.showError("Fetch Failed");
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
            m_state = STATE_DISP_ALERT;
            m_stateTimerMs = currentMillis;
            m_display.renderPoP(m_data.pop);
        } else {
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
        m_state = STATE_DISP_ICON;
        m_stateTimerMs = currentMillis;
        m_display.renderIcon(m_data.currentIcon);
    }
}

void AppFSM::handleInspect(uint32_t currentMillis) {
    // Return back to Present (Now) after inactivity timeout
    if (currentMillis - m_inspectTimerMs >= USER_INSPECT_TIMEOUT_MS) {
        Serial.println("[FSM] Inspect timeout -> returning to Present (Now)");
        m_selectedSlot = 0;
        m_state = STATE_DISP_ICON;
        m_stateTimerMs = currentMillis;
        m_lcd.showForecastSlot(m_data.slots[0]);
        m_display.renderIcon(m_data.slots[0].icon);
        return;
    }

    // Background poll update when due
    if (currentMillis - m_lastPollMs >= POLL_INTERVAL_MS) {
        m_selectedSlot = 0;
        m_state = STATE_FETCH;
        m_stateTimerMs = currentMillis;
    }
}

void AppFSM::handleError(uint32_t currentMillis) {
    if (currentMillis - m_stateTimerMs >= ERROR_RETRY_MS) {
        Serial.println("[FSM] Error backoff elapsed. Retrying fetch...");
        if (!m_weather.isConnected()) {
            m_lcd.showBoot("Reconnecting...");
            m_state = STATE_BOOT;
        } else {
            m_lcd.showFetching();
            m_state = STATE_FETCH;
        }
        m_stateTimerMs = currentMillis;
    }
}

void AppFSM::update(uint32_t currentMillis) {
    // Check hardware button click with debounce
    checkButton(currentMillis);

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
        case STATE_INSPECT:
            handleInspect(currentMillis);
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
