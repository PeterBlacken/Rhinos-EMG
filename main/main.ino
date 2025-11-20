
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <math.h>

// -------- Debug flag --------
#ifndef ENABLE_DEBUG_LOGS
#define ENABLE_DEBUG_LOGS true
#endif

#if ENABLE_DEBUG_LOGS
  #define DBG_PRINT(x)    Serial.print(x)
  #define DBG_PRINTLN(x)  Serial.println(x)
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif

// -------- WiFi / backend --------
const char* WIFI_SSID     = "Wifi_Leufulab";
const char* WIFI_PASSWORD = "LeufuWifi25";

const char* INGEST_URL = "http://192.168.3.92:8000/ingest";
const char* API_KEY    = "leufu25";

// -------- Logical identifiers --------
const char* SUBJECT_ID  = "Demo";
const char* DEVICE_ID   = "GSR01";
const char* SENSOR_TYPE = "GSR";

// -------- Sampling --------
const int   GSR_PIN          = A10;      // analog pin for the GSR divider
const int   SAMPLE_RATE_HZ   = 100;     // 100 Hz
const int   PACK_SAMPLES     = 20;      // samples per packet
const float SAMPLE_PERIOD_MS = 1000.0f / SAMPLE_RATE_HZ;

float     samples_raw[PACK_SAMPLES];
int       sample_index  = 0;
unsigned long last_sample_ms = 0;

// -------- WiFi helpers --------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  DBG_PRINT("Connecting to WiFi");

  unsigned long t0 = millis();
  const unsigned long timeout_ms = 15000;

  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(500);
    DBG_PRINT(".");
  }
  DBG_PRINTLN("");

  if (WiFi.status() == WL_CONNECTED) {
    DBG_PRINT("WiFi connected, IP: ");
    DBG_PRINTLN(WiFi.localIP());
  } else {
    DBG_PRINTLN("WiFi connection FAILED");
  }
}

// -------- HTTP JSON sender --------
void send_packet(unsigned long ts_ms, float resistencia_val) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      DBG_PRINTLN("Skipping packet: no WiFi.");
      return;
    }
  }

  JSONVar doc;
  doc["subject_id"]  = SUBJECT_ID;
  doc["device_id"]   = DEVICE_ID;
  doc["sensor_type"] = SENSOR_TYPE;
  doc["ts"]          = ts_ms;

  JSONVar metrics;
  metrics["resistencia"] = resistencia_val;

  doc["metrics"] = metrics;

  String payload = JSON.stringify(doc);

  WiFiClient client;
  HTTPClient http;
  http.begin(client, INGEST_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", API_KEY);

  int httpCode = http.POST(payload);
  DBG_PRINT("HTTP POST -> code: ");
  DBG_PRINTLN(httpCode);

  if (httpCode > 0) {
    String resp = http.getString();
    if (resp.length() > 0) {
      DBG_PRINTLN(resp);
    }
  } else {
    DBG_PRINTLN("HTTP POST failed.");
  }

  http.end();
}

// -------- Arduino lifecycle --------
void setup() {
  if (ENABLE_DEBUG_LOGS) {
    Serial.begin(115200);
    delay(500);
  }

  connectWiFi();

  DBG_PRINTLN("GSR HTTP sender ready (Arduino_JSON).");
}

void loop() {
  unsigned long now = millis();
  if ((now - last_sample_ms) < SAMPLE_PERIOD_MS) {
    return;
  }
  last_sample_ms = now;

  int raw_adc = analogRead(GSR_PIN);
  samples_raw[sample_index++] = static_cast<float>(raw_adc);

  if (sample_index >= PACK_SAMPLES) {
    unsigned long ts_ms = millis();
    float sum = 0.0f;
    for (int i = 0; i < PACK_SAMPLES; i++) {
      sum += samples_raw[i];
    }
    float resistencia_val = sum / PACK_SAMPLES; // average ADC counts over the packet

    send_packet(ts_ms, resistencia_val);

    sample_index = 0;
  }
}

