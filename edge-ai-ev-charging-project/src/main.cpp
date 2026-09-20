#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"
#include "edge_ai.h"

// ---------------- Globals ----------------
WiFiClient   espClient;
PubSubClient mqttClient(espClient);
DHT          dht(DHT_PIN, DHT_TYPE);

unsigned long lastPublishTime  = 0;
unsigned long lastAIRunTime    = 0;
unsigned long lastBtnArrival   = 0;
unsigned long lastBtnStop      = 0;
unsigned long sessionStartMs   = 0;

bool  bayOccupied   = false;
bool  faultActive   = false;
float lastCurrentA  = 0.0f;
float lastVoltageV  = 0.0f;
float avgCurrentA   = 0.0f;   // simple running average, used as an AI feature

// ---------------- Helpers ----------------
float mapFloat(long x, long inMin, long inMax, float outMin, float outMax) {
    return (x - inMin) * (outMax - outMin) / (float)(inMax - inMin) + outMin;
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID); // Wokwi-GUEST is open — no password arg

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        retries++;
        if (retries > 40) {          // ~20s with no luck -> retry instead of hanging forever
            Serial.println("\nRetrying WiFi.begin()...");
            WiFi.disconnect(true);
            delay(100);
            WiFi.begin(WIFI_SSID);
            retries = 0;
        }
    }
    Serial.println("\nWiFi connected!");

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to ThingsBoard...");
        if (mqttClient.connect("ESP32_EV_OPTIMIZER", TB_TOKEN, NULL)) {
            Serial.println("connected!");
        } else {
            Serial.print("failed, rc=");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

void updateClockFeatures() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
        g_hourOfDay = timeinfo.tm_hour;
        g_dayOfWeek = timeinfo.tm_wday; // 0=Sunday
    }
    // If NTP hasn't synced yet (e.g. first few seconds), keep prior values —
    // g_hourOfDay/g_dayOfWeek default to sane values set in edge_ai.cpp.
}

void readSensors() {
    int rawCurrent = analogRead(CURRENT_PIN); // 0-4095
    int rawVoltage = analogRead(VOLTAGE_PIN); // 0-4095

    // Scale the simulated pot readings to a plausible EV AC charging range.
    // Tune these ranges to match whatever your report claims the hardware is.
    lastCurrentA = mapFloat(rawCurrent, 0, 4095, 0.0f, 32.0f);   // 0-32 A
    lastVoltageV = mapFloat(rawVoltage, 0, 4095, 180.0f, 250.0f); // 180-250 V

    // Exponential moving average -> smoother AI input than raw noisy reading
    avgCurrentA = (0.8f * avgCurrentA) + (0.2f * lastCurrentA);
}

void handleButtons() {
    unsigned long now = millis();

    if (digitalRead(BTN_ARRIVAL) == LOW && (now - lastBtnArrival) > DEBOUNCE_MS) {
        lastBtnArrival = now;
        if (!bayOccupied && !faultActive) {
            bayOccupied     = true;
            sessionStartMs  = now;
            Serial.println("[EVENT] EV arrived -> charging session started");
        }
    }

    if (digitalRead(BTN_STOP) == LOW && (now - lastBtnStop) > DEBOUNCE_MS) {
        lastBtnStop = now;
        if (bayOccupied) {
            bayOccupied = false;
            Serial.println("[EVENT] Session stopped by operator");
        }
        faultActive = false; // manual button also clears a fault (reset)
    }
}

void updateSafetyLogic(float temperatureC) {
    if (!isnan(temperatureC) && temperatureC >= TEMP_FAULT_C) {
        if (!faultActive) {
            Serial.println("[SAFETY] Over-temperature -> charging halted");
        }
        faultActive = true;
        bayOccupied = false; // force-stop charging on fault
    }
}

void updateLEDs() {
    digitalWrite(LED_FAULT,     faultActive ? HIGH : LOW);
    digitalWrite(LED_CHARGING,  (bayOccupied && !faultActive) ? HIGH : LOW);
    digitalWrite(LED_AVAILABLE, (!bayOccupied && !faultActive) ? HIGH : LOW);
}

void publishTelemetry(float temperatureC, float humidity) {
    float sessionElapsedMin = bayOccupied
        ? (millis() - sessionStartMs) / 60000.0f
        : 0.0f;

    StaticJsonDocument<384> doc;
    doc["bayId"]              = BAY_ID;
    doc["voltage"]            = lastVoltageV;
    doc["current"]            = lastCurrentA;
    doc["power"]              = lastVoltageV * lastCurrentA;
    doc["ambientTempC"]       = temperatureC;
    doc["humidity"]           = humidity;
    doc["bayOccupied"]        = bayOccupied;
    doc["fault"]              = faultActive;
    doc["sessionElapsedMin"]  = sessionElapsedMin;
    doc["arrivalProbability"] = g_arrivalProbability;
    doc["predictedDurationMin"] = g_predictedDurationMin;

    char payload[384];
    size_t n = serializeJson(doc, payload);

    Serial.print("Publishing telemetry: ");
    Serial.println(payload);
    mqttClient.publish("v1/devices/me/telemetry", payload, n);
}

// ---------------- Arduino entry points ----------------
void setup() {
    Serial.begin(115200);
    Serial.print("TOKEN IN USE: [");
    Serial.print(TB_TOKEN);
    Serial.println("]");

    dht.begin();

    pinMode(BTN_ARRIVAL, INPUT_PULLUP);
    pinMode(BTN_STOP,    INPUT_PULLUP);
    pinMode(LED_FAULT,     OUTPUT);
    pinMode(LED_CHARGING,  OUTPUT);
    pinMode(LED_AVAILABLE, OUTPUT);

       connectWiFi();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    //connectMQTT();
}


void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    handleButtons();
    readSensors();

    float temperatureC = dht.readTemperature();
    float humidity      = dht.readHumidity();
    if (isnan(temperatureC)) {
        Serial.println("Failed to read from DHT22 (temperature)");
    }

    updateSafetyLogic(temperatureC);
    updateLEDs();
    updateClockFeatures();

    // Feed live state into the edge-AI feature vector
    g_bayOccupied       = bayOccupied;
    g_recentAvgCurrent  = avgCurrentA;
    g_sessionElapsedMin = bayOccupied ? (millis() - sessionStartMs) / 60000.0f : 0.0f;

    unsigned long now = millis();

    if (now - lastAIRunTime >= AI_INFERENCE_INTERVAL) {
        lastAIRunTime = now;
        runEdgeAIInference();
        Serial.print("[AI] arrivalProbability="); Serial.print(g_arrivalProbability, 3);
        Serial.print("  predictedDurationMin=");   Serial.println(g_predictedDurationMin, 1);
    }

    if (now - lastPublishTime >= TELEMETRY_INTERVAL) {
        lastPublishTime = now;
        publishTelemetry(temperatureC, humidity);
    }
}
