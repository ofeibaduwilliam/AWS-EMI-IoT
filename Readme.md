# Basic Temperature & Humidity Telemetry Station

An ESP32 reads a DHT11 sensor, applies offset/gain calibration, and streams JSON
telemetry securely to **AWS IoT Core** over MQTT/TLS. Live data is viewed in the
AWS MQTT Test Client and validated against a reference instrument.

---

## Course

| | |
|---|---|
| **Course** | EE 288 — Electrical Measurements and Instrumentation |
| **Instructor** | Dr. Griffith Selorm Klogo |
| **Institution** | Kwame Nkrumah University of Science and Technology (KNUST) |

### Group members

| Name | Index number |
|---|---|
| William `[fill in your index number]` | `[fill in]` |
| Nana Yaw Asumadu Ntiamoah | 4092624 |
| Jude Allotey Adotei | 4088924 |

---

## Objective

Interface a digital sensor, calibrate it against a reference thermometer, and
stream data securely to the cloud. Specifically:

- Read temperature and humidity from a DHT11.
- Apply a simple offset/gain calibration.
- Publish JSON (device ID, timestamp, temperature, humidity, units) via MQTT to
  AWS IoT Core every 5–10 s.
- View live data in the AWS IoT MQTT Test Client.
- Compare cloud values against a local reference instrument and calculate error.

---

## Hardware

- ESP32 development board
- DHT11 temperature/humidity sensor
- Breadboard and jumper wires
- 10 kΩ resistor (pull-up, only if using a raw 4-pin DHT11)

### Wiring

| DHT11 | ESP32 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| DATA | GPIO4 |

Power the sensor from **3.3 V** so the DATA line stays at 3.3 V logic, safe for
the ESP32. Raw 4-pin modules need a 10 kΩ pull-up between DATA and VCC; most
3-pin breakout modules already include it.

---

## Repository structure

```
telemetry_station/
├── telemetry_station.ino   Main firmware (WiFi + DHT11 + JSON)
├── secrets.h               WiFi creds + device certs — NEVER committed
└── README.md               This file
```

`secrets.h` must be listed in `.gitignore`.

---

## Build & flash (Arduino IDE)

1. Install the **ESP32 board package** (Boards Manager) and select your board and
   serial port.
2. Install libraries (Library Manager):
   - **DHT sensor library** (Adafruit)
   - **Adafruit Unified Sensor** (pulled in as a dependency)
3. Create `secrets.h` from the template and fill in your WiFi SSID and password.
4. Keep `telemetry_station.ino` and `secrets.h` in a folder named
   `telemetry_station/` (Arduino requires the `.ino` to match its folder name).
5. Upload, then open the Serial Monitor at **115200 baud**.

Expected output:

```
Device ID: esp32-3c71bf2a4d10
Connecting to <ssid>....
Connected. IP: 192.168.1.42  RSSI: -58 dBm
Syncing time.....
Time synced: 1755792000
{"device_id":"esp32-3c71bf2a4d10","timestamp":1755792000,"temperature":28.0,"temperature_unit":"C","humidity":65.0,"humidity_unit":"%"}
```

---

## JSON payload

```json
{
  "device_id": "esp32-3c71bf2a4d10",
  "timestamp": 1755792000,
  "temperature": 28.0,
  "temperature_unit": "C",
  "humidity": 65.0,
  "humidity_unit": "%"
}
```

| Field | Type | Description |
|---|---|---|
| `device_id` | string | Derived from the ESP32 MAC; stable per board, reused as the AWS Thing name |
| `timestamp` | integer | UTC epoch seconds, NTP-synced |
| `temperature` | float | Calibrated temperature |
| `temperature_unit` | string | `C` (degrees Celsius) |
| `humidity` | float | Calibrated relative humidity |
| `humidity_unit` | string | `%` (percent RH) |

---

## Security

- **TLS + X.509 certificates** for the AWS IoT Core connection (MQTT over port 8883).
- **No hard-coded secrets.** WiFi credentials, the device certificate, and the
  private key all live in `secrets.h`, which is excluded from version control.
- Each device authenticates with its own certificate under a least-privilege IoT
  policy scoped to its own topics.

---

## Calibration

The firmware applies a linear correction to each raw reading:

```
calibrated = raw * gain + offset
```

Gain and offset are set to 1.0 and 0.0 (identity) until a two-point calibration
is performed:

1. Record raw sensor readings alongside a reference instrument at two known points.
2. Solve for `gain` and `offset` from the two (raw, reference) pairs.
3. Update the constants in the firmware and re-flash.

---

## Measurement characteristics & uncertainty

### DHT11 datasheet specification

| Quantity | Range | Resolution | Accuracy |
|---|---|---|---|
| Temperature | 0–50 °C | 1 °C | ±2 °C |
| Humidity | 20–90 %RH | 1 %RH | ±5 %RH |

Maximum sampling rate ≈ 1 Hz (minimum ~1–2 s between reads).

### Sources of error

- **Quantisation (resolution):** integer output discards the fractional part,
  giving a ±0.5 °C / ±0.5 %RH rounding error.
- **Sensor accuracy:** intrinsic ±2 °C / ±5 %RH datasheet tolerance.
- **Self-heating** of the sensor and nearby board components.
- **Thermal lag / response time:** the reading trails rapid ambient changes.
- **Placement:** airflow, sunlight, and proximity to heat sources.
- **Humidity hysteresis** and long-term calibration drift.
- **Reference instrument uncertainty:** the thermometer/multimeter used for
  comparison has its own tolerance, which must be included.

### Illustrative uncertainty budget (Type B, from datasheet)

Treating each bound as a rectangular distribution, the standard uncertainty of a
half-width *a* is *u = a / √3*.

| Component | Half-width | Standard uncertainty *u* |
|---|---|---|
| Temperature — resolution | 0.5 °C | 0.29 °C |
| Temperature — accuracy | 2 °C | 1.15 °C |
| **Temperature — combined** | | **≈ 1.19 °C** |
| Humidity — resolution | 0.5 %RH | 0.29 %RH |
| Humidity — accuracy | 5 %RH | 2.89 %RH |
| **Humidity — combined** | | **≈ 2.90 %RH** |

Expanded uncertainty at *k = 2* (≈ 95 % confidence): roughly **±2.4 °C** and
**±5.8 %RH**. These are illustrative; the reference instrument's uncertainty
should be combined in for the final figure.

---

## Validation procedure

1. Place the DHT11 and the reference instrument in the same environment.
2. Read the live cloud value in the AWS IoT MQTT Test Client.
3. Record the reference reading at the same moment.
4. Compute error = cloud value − reference value; log across several points and
   temperatures.
5. Use the results to set the calibration gain/offset, then repeat to confirm.

---

## AWS Free Tier & cleanup

- The message volume here sits comfortably within the AWS IoT Core Free Tier.
- **Clean up after each lab:** delete the Thing, detach and delete the
  certificate, and remove the IoT policy to avoid leaving live credentials or
  incurring charges.
- Keep a billing alert configured on the account.

---

## Project status

| Layer | Status |
|---|---|
| ESP32 + WiFi + DHT11 → serial JSON | ✅ Done |
| Device ID, NTP timestamp, units, secrets split out | ✅ Done |
| AWS IoT Core: Thing / certificate / policy setup | ⬜ Next |
| MQTT over TLS publish to AWS IoT Core | ⬜ Pending |
| MQTT Test Client verification | ⬜ Pending |
| Calibration + error documentation | ⬜ Pending |
