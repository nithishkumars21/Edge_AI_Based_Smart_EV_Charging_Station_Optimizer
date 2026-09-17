# Edge AI Based Smart EV Charging Station Optimizer

Final project for the IoT (Embedded IoT Programming) internship at Emertxe Information Technologies, Bangalore — Cohort EI26_017.

A single-bay EV charging controller, simulated on an ESP32 in Wokwi, that reports live sensor telemetry to a ThingsBoard Cloud dashboard and runs two small machine-learning models directly on the microcontroller to support charging decisions.

**Author:** Nithishkumar S — EGS Pillay Engineering College
**Contact:** e23ecr064@egspec.org

---

## What this actually is

The system reads simulated voltage and current (via potentiometers), ambient temperature and humidity (via a DHT22), and two push buttons for EV arrival/stop. It drives three status LEDs (available / charging / fault), enforces a hard over-temperature safety cutoff, and publishes everything — including two on-device AI predictions — to ThingsBoard Cloud over MQTT.

It's best described as a decision-support layer for a single charging bay, not an autonomous multi-bay scheduler. That distinction is intentional — see [Limitations](#limitations) below.

## Features

- ESP32 firmware with a clean state machine: `AVAILABLE → CHARGING → FAULT`
- Simulated sensing: 2x potentiometer (current/voltage), DHT22 (temperature/humidity)
- Two-button interface: EV arrival/start, stop/emergency reset
- Hardware-independent safety interlock (over-temperature force-stops charging regardless of what the AI predicts)
- Two Edge AI models running on-device, with no server round-trip:
  - **Arrival probability** — Logistic Regression, exported to C via [m2cgen](https://github.com/BayesWitnesses/m2cgen)
  - **Session duration** — Decision Tree Regressor, exported to C via [micromlgen](https://github.com/eloquentarduino/micromlgen)
- MQTT telemetry to ThingsBoard Cloud every 5 seconds
- Full Wokwi circuit simulation — no physical hardware required to run this

## Architecture

```
Sensors & Buttons  →  ESP32 Firmware  →  Edge AI Inference  →  MQTT Client  →  ThingsBoard Cloud
(2x pot, DHT22,        (sensing, safety    (arrival + duration    (PubSubClient      (live dashboard)
 2x pushbutton)          logic, LEDs)        models, on-device)     over WiFi)
```

Sensor and button state is read every loop cycle. Every 10 seconds the two Edge AI models run on the current feature vector. Every 5 seconds the full telemetry payload — including both AI predictions — is published over MQTT.

## Hardware / Wiring (simulated in Wokwi)

| ESP32 Pin | Signal | Connected To |
|---|---|---|
| GPIO34 | `CURRENT_PIN` (ADC) | Potentiometer 2 — SIG |
| GPIO35 | `VOLTAGE_PIN` (ADC) | Potentiometer 1 — SIG |
| GPIO15 | `DHT_PIN` | DHT22 — SDA |
| GPIO32 | `BTN_ARRIVAL` (INPUT_PULLUP) | Push button 1 (green) |
| GPIO33 | `BTN_STOP` (INPUT_PULLUP) | Push button 2 (red) |
| GPIO21 | `LED_FAULT` | 1k resistor → LED1 (red) → GND |
| GPIO19 | `LED_CHARGING` | 1k resistor → LED2 (yellow) → GND |
| GPIO18 | `LED_AVAILABLE` | 1k resistor → LED3 (green) → GND |

Full wiring is defined in `diagram.json` (Wokwi format).

## Getting started

### Requirements
- [VS Code](https://code.visualstudio.com/)
- [PlatformIO IDE](https://platformio.org/) extension
- [Wokwi Simulator](https://wokwi.com/) extension (free Wokwi account for a license key)
- A free [ThingsBoard Cloud](https://thingsboard.cloud/) account (confirm your region — EU and NA are separate servers with separate MQTT endpoints)

### Run it
1. Clone this repository and open the folder in VS Code.
2. Let PlatformIO detect `platformio.ini` and finish its first-time setup.
3. In `src/config.cpp`, replace `TB_TOKEN` with your own ThingsBoard device's Access Token, and set `MQTT_SERVER` to match your account's region (e.g. `mqtt.eu.thingsboard.cloud` for EU Cloud accounts, `thingsboard.cloud` for the default/NA instance).
4. Build the project (PlatformIO checkmark).
5. Press `F1` → **Wokwi: Start Simulator**.
6. Watch the Serial Monitor for WiFi connect → MQTT connect → AI inference logs → telemetry publishes.

### ThingsBoard dashboard
Create a device, copy its Access Token into `config.cpp`, then build a dashboard with widgets bound to these telemetry keys: `voltage`, `current`, `power`, `ambientTempC`, `humidity`, `bayOccupied`, `fault`, `sessionElapsedMin`, `arrivalProbability`, `predictedDurationMin`, `bayId`.

## Edge AI methodology

Both models share a 6-feature input vector: `hourOfDay`, `dayOfWeek`, `bayOccupied`, `recentAvgCurrent`, `sessionElapsedMin`, `historicalArrivalRate`.

They're trained offline in Python (`tools/train_models.py`) using scikit-learn, then exported as plain C functions — no ML runtime is needed on the microcontroller.

**Honest disclosure:** both models are currently trained on a synthetic dataset (generated to mimic realistic arrival/duration patterns), not on real logged usage. This is stated as a limitation, not hidden. To retrain on real data: export historical telemetry from ThingsBoard as CSV, swap it into `generate_dataset()` in `train_models.py`, and re-run the script.

## Testing

| # | Test | Expected result |
|---|---|---|
| 1 | Power on, idle | Green LED on; dashboard shows bay unoccupied |
| 2 | Press arrival button | Yellow LED on; session timer starts climbing |
| 3 | Raise DHT22 temperature above 45°C | Red LED on; charging force-stopped; fault flag set |
| 4 | Press stop button | Fault clears; system returns to available |
| 5 | Watch dashboard for 60+ seconds | All telemetry fields and AI predictions update live |

## Limitations

- ML models trained on synthetic, not real, usage data
- Voltage/current are potentiometer proxies, not real sensors — a Wokwi simulation substitution
- Single bay only — no multi-bay queueing or scheduling logic
- No battery-backed RTC; `hourOfDay`/`dayOfWeek` depend on NTP sync after WiFi connects
- No MQTT-over-TLS or per-device provisioning beyond a static access token

## Future scope

- Retrain models on real ThingsBoard-logged usage data
- Extend to multiple bays with an actual queueing/scheduling algorithm
- Replace potentiometers with real current/voltage sensor ICs on physical hardware
- Add MQTT over TLS and per-device provisioning

## License

Not currently licensed for reuse. Add a license file here if you intend this repository to be reused by others.
