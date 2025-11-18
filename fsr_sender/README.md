# GSR HTTP sender (Arduino)

This sketch streams Galvanic Skin Response (GSR) readings to the backend using the JSON shape expected by the ingest service.

## Hardware / pinout
- **GSR analog input:** `GSR_PIN` (default `A0`). Connect the GSR divider output to this analog pin.
- **Board:** ESP32 (uses `WiFi.h`/`HTTPClient.h`).

## Build flags
- `ENABLE_DEBUG_LOGS` (default `true`): controls `Serial` debug output. Define as `false` at compile time to silence logs, e.g.
`-DENABLE_DEBUG_LOGS=false`.

## Network / backend config
Edit the constants near the top of `fsr_sender.ino`:
- `WIFI_SSID` / `WIFI_PASSWORD`
- `INGEST_URL` (HTTP endpoint `/ingest`)
- `API_KEY` (`X-Api-Key` header)
- Logical identifiers: `SUBJECT_ID`, `DEVICE_ID`, `SENSOR_TYPE` (defaults set to the GSR backend expectations)

## Sampling cadence
- Sampling rate: **100 Hz** (one sample every 10 ms).
- Raw packet: **20 samples** (`PACK_SAMPLES`) averaged into a single reading.

## Payload shape
Matches the backend's expected GSR payload:
```json
{
  "subject_id": "Demo",
  "device_id": "GSR01",
  "sensor_type": "GSR",
  "ts": 1763480203245,
  "metrics": {
    "resistencia": 0
  }
}
```

## Error handling
The sketch reuses the EMG sender's approach: it attempts a single HTTP POST per packet with default `HTTPClient` timeouts and will log failures when debugging is enabled.
