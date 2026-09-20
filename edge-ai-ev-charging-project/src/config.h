#pragma once

// ---------------- WiFi ----------------
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// ---------------- ThingsBoard ----------------
extern const char* MQTT_SERVER;
extern const int   MQTT_PORT;
extern const char* TB_TOKEN;   // Device Access Token from YOUR ThingsBoard device
extern const char* BAY_ID;     // Charging bay identifier, e.g. "BAY1"

// ---------------- Pin map (matches diagram.json) ----------------
#define CURRENT_PIN   34   // pot2 -> simulated charging current sensor
#define VOLTAGE_PIN   35   // pot1 -> simulated charging voltage sensor
#define DHT_PIN       15   // dht1 -> ambient temperature/humidity
#define DHT_TYPE      DHT22

#define BTN_ARRIVAL   32   // btn1 (green) -> simulate EV plug-in / start charging
#define BTN_STOP      33   // btn2 (red)   -> simulate unplug / stop / emergency

#define LED_FAULT     21   // led1 (red)    -> overheat / fault
#define LED_CHARGING  19   // led2 (yellow) -> actively charging
#define LED_AVAILABLE 18   // led3 (green)  -> bay free / ready

// ---------------- Thresholds (tune these; document your choice in the report) ----------------
#define TEMP_FAULT_C        45.0f   // ambient temp above this -> fault, stop charging
#define TELEMETRY_INTERVAL  5000UL  // ms between MQTT publishes
#define AI_INFERENCE_INTERVAL 10000UL // ms between edge-AI inference runs
#define DEBOUNCE_MS         200UL
