# ESP8266 Ultrasonic Water Level & Environmental Sensor

A modular, high-accuracy ESP8266 NodeMCU firmware built with **PlatformIO**. It measures roof tank water level using ultrasonic distance sensing, monitors ambient temperature and humidity, provides an embedded Web UI Dashboard with Web OTA firmware upload capabilities, and publishes telemetry via MQTT.

---

## Key Features

- **Outlier Noise Filtering & Smoothing**:
  - 9-ping insertion-sort median filter rejects spurious reflections (`<= 1 cm` or `> tank_bottom + 50 cm`).
  - Exponential Moving Average (EMA) smoothing ($\alpha = 0.25$) ensures stable water level readings.
- **Environmental Sensing**:
  - Supports BME280 (I2C `0x76` / `0x77`) and DHT11 (`D5`) for ambient temperature (°C), relative humidity (%), and barometric pressure.
- **Embedded Dark-Mode Web Dashboard**:
  - Live telemetry cards for distance, water depth, percentage fill bar, temperature, humidity, and RSSI.
  - Interactive Web OTA uploader (`.bin` file upload with live progress bar).
  - Wi-Fi network credential reset button.
- **1-Minute Deep Sleep Cycle**:
  - Automatically enters 1-minute (`60` seconds) Deep Sleep after each telemetry publish cycle.
  - Automatically skips the 3-minute boot window on routine timer wake-ups to save power.
- **MQTT Remote OTA Control & Telemetry**:
  - Publishes real-time telemetry to MQTT broker.
  - Listen to MQTT control commands to activate **Stay-Awake / OTA Maintenance Mode** remotely while in Deep Sleep.

---

## MQTT Command & Telemetry Specification

### MQTT Broker Defaults
- **Host**: `YOUR_MQTT_BROKER_IP` (e.g., `192.168.1.100`)
- **Port**: `1883`

### 1. Remote OTA & Maintenance Control Topic
- **Topic**: `roof/tank_water/control`

| Command / Payload | Description |
| :--- | :--- |
| `"ota"` or `"stay_awake"` or `"update"` | Cancels Deep Sleep and keeps the device awake in **Web UI mode for 10 minutes** so you can access `http://<device-ip>/` and upload a new `firmware.bin`. |
| `"sleep"` | Immediately cancels Stay-Awake mode and forces the ESP8266 into Deep Sleep. |
| `"debug_on"` or `"debug_enable"` | Enables real-time diagnostic logging published to `roof/sensor/log`. |
| `"debug_off"` or `"debug_disable"` | Disables diagnostic logging to `roof/sensor/log`. |

#### Example Command Line Usage (`mosquitto_pub`):
```bash
# Wake up sensor remotely for 10 minutes to update firmware via Web UI
mosquitto_pub -h YOUR_MQTT_BROKER_IP -t "roof/tank_water/control" -m "ota"

# Return to sleep immediately
mosquitto_pub -h YOUR_MQTT_BROKER_IP -t "roof/tank_water/control" -m "sleep"
```

---

### 2. Telemetry Topics Published by Device

| Topic | Data Format | Description | Example |
| :--- | :--- | :--- | :--- |
| `roof/tank_water/level` | String (Float) | Calculated water level percentage | `"85.4"` |
| `roof/tank_water/distance` | String (Integer) | Filtered distance to water in cm | `"25"` |
| `roof/sensor/temperature` | String (Float) | Ambient Temperature in °C | `"28.50"` |
| `roof/sensor/humidity` | String (Float) | Relative Humidity in % | `"62.10"` |
| `roof/sensor/log` | String | Detailed diagnostic calculation logs (raw median, EMA, tank calculations) | `"DIAG: raw_median=25 cm, smoothed_dist=25 cm..."` |
| `roof/log` | String | System status and OTA activation logs | `"OTA Mode Active: Staying awake..."` |

---

## Operating Modes & Deep Sleep Timing

### 1. Deep Sleep Mode (Normal Battery/Solar Operation)
- **Cycle**: Every **1 minute** (60 seconds).
- **Process**: Wakes up $\rightarrow$ Measures distance & env data $\rightarrow$ Publishes MQTT telemetry $\rightarrow$ Listens 2 seconds for incoming control payloads $\rightarrow$ Enters `ESP.deepSleep(60e6)`.

### 2. Stay-Awake / Maintenance Mode
- **Triggers**:
  1. Publishing `"ota"` to `roof/tank_water/control` via MQTT (10-minute window).
  2. Initial hardware power-on / reset (3-minute boot window).
- **Frequency**:
  - Web UI Dashboard: Refresh live readings **every 2 seconds**.
  - MQTT Telemetry: Publishes updated values **every 4 seconds**.

---

## Hardware Wiring Guide (NodeMCU v2)

| ESP8266 Pin | Connected Component | Function |
| :--- | :--- | :--- |
| `D2` (GPIO4) | HC-SR04 Trigger | Ultrasonic Pulse Output |
| `D1` (GPIO5) | HC-SR04 Echo | Ultrasonic Pulse Input |
| `D5` (GPIO14) | DHT11 Signal | Temperature / Humidity Data |
| `D2` (SDA) / `D1` (SCL) | BME280 Module | I2C Sensor Bus (`0x76` / `0x77`) |
| `D0` / `D4` | Built-in LED | Visual Indicator / Heartbeat |

---

## Web UI & API Endpoints

Access the Web Dashboard by navigating to `http://<device-ip>/` in any web browser.

- `GET /`: Responsive Dark-Mode Telemetry & Control Dashboard.
- `GET /api/readings`: Returns JSON telemetry payload:
  ```json
  {
    "distance_to_water": 25,
    "water_level": 125,
    "percentage": 83.3,
    "temperature": 28.5,
    "humidity": 62.1,
    "mqtt_connected": true,
    "stay_awake_remaining_sec": 540,
    "uptime_sec": 120,
    "wifi_rssi": -55
  }
  ```
- `POST /api/config`: Save parameters (`s` = speed of sound, `t` = tank bottom distance, `m` = max water level).
- `POST /update`: Multipart form endpoint for uploading compiled `firmware.bin`.
- `POST /api/wifi_reset`: Erases Wi-Fi credentials and launches `WATER_LEVEL_SENSOR` setup Access Point.

---

## Building and Flashing with PlatformIO

### Build Firmware
```powershell
pio run
```

### Run Unit Tests
```powershell
pio test
```

### Flash Device via USB (COM Port)
```powershell
pio run -t upload
```

### Monitor Serial Console Output
```powershell
pio device monitor -b 115200
```
