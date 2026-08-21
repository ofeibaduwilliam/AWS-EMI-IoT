/*
 * Basic Temperature & Humidity Telemetry Station
 * Layer 1: WiFi + DHT11 -> calibrated JSON to serial
 *
 * Board: ESP32   Sensor: DHT11 on GPIO4
 * Cloud layer (AWS IoT Core over MQTT/TLS) is added next.
 *
 * Secrets (WiFi creds, later device certs) live in secrets.h, which is
 * kept OUT of version control. See secrets.h in this folder.
 */

#include <WiFi.h>
#include <DHT.h>
#include <time.h>
#include "secrets.h"          // defines WIFI_SSID, WIFI_PASSWORD

// ---------- Sensor ----------
#define DHT_PIN   4           // DATA line -> GPIO4
#define DHT_TYPE  DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ---------- Calibration:  calibrated = raw * gain + offset ----------
// Identity for now. Tune AFTER comparing to your reference thermometer.
const float TEMP_GAIN   = 1.0;
const float TEMP_OFFSET = 0.0;
const float HUM_GAIN    = 1.0;
const float HUM_OFFSET  = 0.0;

// ---------- Timing ----------
const unsigned long PUBLISH_INTERVAL_MS = 5000;   // 5 s
unsigned long lastPublish = 0;

// ---------- Identity ----------
char deviceId[24];            // derived from the ESP32 MAC, stable per board

void makeDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceId, sizeof(deviceId), "esp32-%012llx", (unsigned long long)mac);
}

void connectWiFi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected. IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// Sync clock from NTP so payloads carry a real epoch timestamp (UTC).
// Bounded wait; if it fails we flag it rather than block forever.
bool syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // UTC, no offset
  Serial.print("Syncing time");
  for (int i = 0; i < 20; i++) {                        // up to ~10 s
    if (time(nullptr) > 1700000000) {                   // clearly past epoch
      Serial.printf("\nTime synced: %ld\n", (long)time(nullptr));
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWARNING: NTP sync failed, timestamps will be 0");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  makeDeviceId();
  Serial.printf("Device ID: %s\n", deviceId);
  dht.begin();
  connectWiFi();
  syncTime();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  unsigned long now = millis();
  if (now - lastPublish < PUBLISH_INTERVAL_MS) return;
  lastPublish = now;

  float rawT = dht.readTemperature();   // degrees Celsius
  float rawH = dht.readHumidity();      // % relative humidity

  if (isnan(rawT) || isnan(rawH)) {
    Serial.println("{\"error\":\"DHT read failed\"}");
    return;
  }

  float tempC    = rawT * TEMP_GAIN + TEMP_OFFSET;
  float humidity = rawH * HUM_GAIN  + HUM_OFFSET;
  long  ts       = (long)time(nullptr);   // epoch seconds, UTC

  // Same JSON shape we'll publish to AWS IoT Core: device ID, epoch
  // timestamp, values with explicit units.
  char payload[200];
  snprintf(payload, sizeof(payload),
    "{\"device_id\":\"%s\",\"timestamp\":%ld,"
    "\"temperature\":%.1f,\"temperature_unit\":\"C\","
    "\"humidity\":%.1f,\"humidity_unit\":\"%%\"}",
    deviceId, ts, tempC, humidity);

  Serial.println(payload);
}
