# FSR HTTP sender (Arduino)

This sketch streams Force Sensitive Resistor (FSR) readings to the backend using the same JSON shape as the EMG sender.

## Hardware / pinout
- **FSR analog input:** `FSR_PIN` (default `A0`). Connect the FSR in a voltage divider that feeds this analog pin.
- **Board:** ESP32 (uses `WiFi.h`/`HTTPClient.h`).

## Build flags
- `ENABLE_DEBUG_LOGS` (default `true`): controls `Serial` debug output. Define as `false` at compile time to silence logs, e.g. `-DENABLE_DEBUG_LOGS=false`.

## Network / backend config
Edit the constants near the top of `fsr_sender.ino`:
- `WIFI_SSID` / `WIFI_PASSWORD`
- `INGEST_URL` (HTTP endpoint `/ingest`)
- `API_KEY` (`X-Api-Key` header)
- Logical identifiers: `SUBJECT_ID`, `DEVICE_ID`, `SENSOR_TYPE`

## Sampling & feature cadence
- Sampling rate: **100 Hz** (one sample every 10 ms).
- Raw packet: **20 samples** (`PACK_SAMPLES`) per POST.
- Features (`mean`, `rms`, `peak`) sent every **10 packets** (`FEATURE_DECIMATE`), mirroring the EMG decimation pattern (features every 2 seconds with default settings).

## Payload shape
Matches `main/main.ino`:
```json
{
  "subject_id": "...",
  "device_id": "...",
  "sensor_type": "FSR",
  "ts": 1700000000,
  "metrics": {
    "raw": [ /* 20 analog readings */ ],
    "mean": 0.0,
    "rms": 0.0,
    "peak": 0.0
  }
}
```
`mean`/`rms`/`peak` appear only on decimated packets.

## Error handling
The sketch reuses the EMG sender's approach: it attempts a single HTTP POST per packet with default `HTTPClient` timeouts and will log failures when debugging is enabled.
