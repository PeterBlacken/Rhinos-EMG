#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <ADS1015_WE.h>
#include <Arduino_JSON.h>
#include <math.h>

// ---------- I2C / ADS1015 ----------
#define I2C_ADDRESS 0x48
ADS1015_WE adc(I2C_ADDRESS);

const char* WIFI_SSID     = "Wifi_Leufulab";
const char* WIFI_PASSWORD = "LeufuWifi25";

// Cambia esto por la IP de tu backend
const char* INGEST_URL = "http://192.168.3.79:8000/ingest";
const char* API_KEY    = "leufu25";          // debe coincidir con el backend

// Campos lógicos del JSON
const char* SUBJECT_ID  = "RAW_TEST";
const char* DEVICE_ID   = "emg_raw";
const char* SENSOR_TYPE = "EMG_RAW";

// ---------- EMG / paquetes ----------
const int SAMPLE_RATE_HZ   = 3300;   // objetivo, solo referencia
const int PACK_SAMPLES     = 20;     // nº de muestras por paquete ("raw")
const int FEATURE_DECIMATE = 10;     // enviar mean/rms/peak cada 10 paquetes

float     samples_uv[PACK_SAMPLES];  // buffer en microvolts
int       sample_index   = 0;
uint32_t  packet_index   = 0;

// ---------- WiFi helpers ----------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  unsigned long t0 = millis();
  const unsigned long timeout_ms = 15000;

  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection FAILED");
  }
}

// ---------- ADS1015 helpers ----------
void setupADC() {
  if (!adc.init()) {
    Serial.println("ADS1015 not connected!");
    while (true) {
      delay(1000);
    }
  }

  adc.setVoltageRange_mV(ADS1015_RANGE_6144);
  adc.setConvRate(ADS1015_3300_SPS);
  adc.setMeasureMode(ADS1015_CONTINUOUS);
  adc.setCompareChannels(ADS1015_COMP_0_GND);

  Serial.println("ADS1015 ready in continuous mode @3300 SPS.");
}

// ---------- Feature computation ----------
void compute_features(float &mean_uv, float &rms_uv, float &peak_uv) {
  float sum    = 0.0f;
  float sum_sq = 0.0f;
  float max_abs = 0.0f;

  for (int i = 0; i < PACK_SAMPLES; i++) {
    float x = samples_uv[i];
    sum    += x;
    sum_sq += x * x;
    float ax = fabsf(x);
    if (ax > max_abs) max_abs = ax;
  }

  mean_uv = sum / PACK_SAMPLES;
  rms_uv  = sqrtf(sum_sq / PACK_SAMPLES);
  peak_uv = max_abs;
}

// ---------- HTTP JSON with Arduino_JSON ----------
void send_packet(unsigned long ts_ms, bool send_features) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Skipping packet: no WiFi.");
      return;
    }
  }

  float mean_uv = 0.0f;
  float rms_uv  = 0.0f;
  float peak_uv = 0.0f;

  if (send_features) {
    compute_features(mean_uv, rms_uv, peak_uv);
  }

  // Build JSON using Arduino_JSON (JSONVar)
  JSONVar doc;
  doc["subject_id"]  = SUBJECT_ID;
  doc["device_id"]   = DEVICE_ID;
  doc["sensor_type"] = SENSOR_TYPE;
  doc["ts"]          = ts_ms;   // ahora es unsigned long, compatible

  JSONVar metrics;
  JSONVar raw;

  for (int i = 0; i < PACK_SAMPLES; i++) {
    raw[i] = samples_uv[i];     // microvolts (float)
  }

  metrics["raw"] = raw;

  if (send_features) {
    metrics["mean_uv"] = mean_uv;
    metrics["rms_uv"]  = rms_uv;
    metrics["peak_uv"] = peak_uv;
  }

  doc["metrics"] = metrics;

  String payload = JSON.stringify(doc);

  WiFiClient client;
  HTTPClient http;
  http.begin(client, INGEST_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", API_KEY);

  int httpCode = http.POST(payload);
  Serial.print("HTTP POST -> code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String resp = http.getString();
    if (resp.length() > 0) {
      Serial.println(resp);
    }
  } else {
    Serial.println("HTTP POST failed.");
  }

  http.end();
}

// ---------- Arduino lifecycle ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin();            // XIAO ESP32S3 usa pines por defecto; cambia si hace falta
  Wire.setClock(400000);

  setupADC();
  connectWiFi();

  Serial.println("EMG HTTP sender ready (Arduino_JSON).");
}

void loop() {
  // Leer una muestra del ADS1015 en modo continuo
  float mv = adc.getResult_mV();   // millivoltios
  float uv = mv * 1000.0f;        // microvoltios

  samples_uv[sample_index++] = uv;

  if (sample_index >= PACK_SAMPLES) {
    unsigned long ts_ms = millis();
    bool send_features = (packet_index % FEATURE_DECIMATE == 0);

    send_packet(ts_ms, send_features);

    sample_index = 0;
    packet_index++;
  }

  // delayMicroseconds(300);  // opcional si quieres forzar ritmo desde MCU
}
