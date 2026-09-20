#include "config.h"

// Wokwi's virtual WiFi network — leave as-is for simulation
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ThingsBoard Cloud
const char* MQTT_SERVER = "mqtt.eu.thingsboard.cloud";
const int   MQTT_PORT   = 1883;

// !! REPLACE with the Access Token of YOUR OWN device in ThingsBoard !!
// Do not reuse a token you saw in a shared training file — it belongs to
// someone else's device and you cannot control who else is publishing to it.
const char* TB_TOKEN = "cbuHFkuVnlLSv6ZWVEnD";  // <--- REPLACE with your own device's token
const char* BAY_ID    = "BAY1";
