/*
 * Basic Temperature & Humidity Telemetry Station
 * Layer 2: DHT11 -> calibrated JSON -> AWS IoT Core over MQTT/TLS
 *
 * Board: ESP32   Sensor: DHT11 on GPIO4
 * Secrets (WiFi creds, endpoint, Thing name, certs) live in secrets.h.
 * 
 */
 
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>
#include <WiFi.h>

// Publish topic must match the least-privilege IoT policy:
//   esp32/<THINGNAME>/telemetry
// String-literal concatenation, so THINGNAME must be a #define in secrets.h.
#define PUB_TOPIC ("esp32/" THINGNAME "/telemetry")

// ---------- Sensor ----------
#define DHT_PIN   4
#define DHT_TYPE  DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ---------- Calibration: calibrated = raw * gain + offset ----------
// Identity until you calibrate against your reference thermometer.
const float TEMP_GAIN   = 1.0;
const float TEMP_OFFSET = 0.0;
const float HUM_GAIN    = 1.0;
const float HUM_OFFSET  = 0.0;

// ---------- Timing ----------
const unsigned long PUBLISH_INTERVAL_MS = 10000;   // 10 s

WiFiClientSecure net;
PubSubClient client(net);
unsigned long lastPublish = 0;

// NTP sync. REQUIRED before the TLS handshake: the ESP32 boots with its clock
// at 1970, so TLS rejects the AWS certificate as "not yet valid" and the
// connection silently fails. Also supplies the real epoch timestamp.
bool syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // UTC
  Serial.print("Syncing time");
  for (int i = 0; i < 20; i++) {
    if (time(nullptr) > 1700000000) {
      Serial.printf("\nTime synced: %ld\n", (long)time(nullptr));
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWARNING: NTP sync failed; TLS may reject the certificate.");
  return false;
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to Wi-Fi SSID: [%s]", WIFI_SSID);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.printf(" [status=%d]", WiFi.status());
    if (++tries > 20) {   // ~10 s, then report and keep trying
      Serial.println("\nWi-Fi not connecting. 1=SSID not found, 4=wrong password, 6=disconnected.");
      tries = 0;
    }
  }
  Serial.println(" connected.");

  syncTime();                       // must run before the TLS connect below

  net.setCACert(AWS_ROOT_CA);
  net.setCertificate(DEVICE_CERT);
  net.setPrivateKey(PRIVATE_KEY);

  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setBufferSize(512);        // headroom so the JSON packet isn't dropped

  Serial.print("Connecting to AWS IoT Core");
  while (!client.connect(THINGNAME)) {   // client ID == Thing name (policy needs this)
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" connected.");
}

void publishReading() {
  float rawT = dht.readTemperature();   // degrees Celsius
  float rawH = dht.readHumidity();      // % relative humidity

  if (isnan(rawT) || isnan(rawH)) {
    Serial.println("DHT read failed; skipping this cycle.");
    return;
  }

  float tempC    = rawT * TEMP_GAIN + TEMP_OFFSET;
  float humidity = rawH * HUM_GAIN  + HUM_OFFSET;

  StaticJsonDocument<200> doc;
  doc["device_id"]        = THINGNAME;
  doc["timestamp"]        = (long)time(nullptr);   // UTC epoch seconds
  doc["temperature"]      = tempC;
  doc["temperature_unit"] = "C";
  doc["humidity"]         = humidity;
  doc["humidity_unit"]    = "%";

  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(PUB_TOPIC, buffer);
  Serial.println(buffer);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();
  connectAWS();
}

void loop() {
  if (!client.connected()) {
    connectAWS();
  }
  client.loop();

  if (millis() - lastPublish > PUBLISH_INTERVAL_MS) {
    lastPublish = millis();
    publishReading();
  }
}
