# ESP32 AQI Monitor (PMS5003)
A simple **ESP32-based Air Quality Index (AQI) monitor** using **ESP32 CYD 2.8" TFT Display** and **Plantower PMS5003** particulate matter sensor. The project reads PM1.0, PM2.5, and PM10 concentrations from the sensor, calculates AQI based on **US EPA** or **China MEP** standards, and displays the result with a color-coded air quality category.

## Features
- Real-time PM1.0, PM2.5, and PM10 readings.
- Selectable AQI calculation:
  - **US EPA AQI**
  - **China AQI**
- Display on TFT LCD.
- Displays as: particle counts (0.3–10 µm) and region index (with color).
- Uses **LittleFS** to load custom TFT fonts.

## Hardware
| Component | Notes |
|----------|-------|
| ESP32 CYD 2.8" TFT Display | Use this device for seamless installation |
| Plantower PMS5003 | UART particulate matter sensor |
| MT3608 DC-DC Step Up Power Booster | Set to 5V |
| Momentary/Tack Switch | To switch AQI region (US/CN) |
| 5V Power Supply | Or common USB power source |

## Wiring
### PMS5003 → ESP32
| PMS5003 | ESP32 |
|---------|-------|
| VCC | 5V |
| GND | GND |
| TX | GPIO 27 |
| RX | GPIO 22 |

### Switch (to change AQI calculation, press 2 second to change)
| Switch | ESP32 |
|--------|-------|
| One side | GPIO 22 |
| Other side | GND |

### Diagram
![Diagram](https://github.com/ilyanto/esp32_aqi/blob/main/wiring.jpg)

## TFT Display
This project uses the **TFT_eSPI** library. Configure your display driver and pin mapping inside the TFT_eSPI `User_Setup.h` (or custom setup file) before compiling. Default `User_Setup.h` at the source code is for CYD 2.8" TFT Display.

## AQI Calculation
AQI is calculated from the **PM2.5 atmospheric concentration** reported by the PMS3002.

### Supported Standards
| Region | Description |
|--------|-------------|
| US | US EPA PM2.5 AQI breakpoints |
| CN | China Ambient Air Quality Index breakpoints |

Press the tack switch button to switch between US and China AQI calculation.

### AQI Categories
| AQI | Category |
|----:|----------|
| 0–50 | 🟢 Good |
| 51–100 | 🟡 Moderate |
| 101–150 | 🟠 Unhealthy for Sensitive Groups |
| 151–200 | 🔴 Unhealthy |
| 201–300 | 🟣 Very Unhealthy |
| 301–500 | ⚫ Hazardous |

## Example output
```text
[BIG DISPLAY] AQI:57
[DESCRIPTION] Moderate
[INFO LINE1] US PM1:8|8 PM2.5:14|15 PM10:18|19 µg/m3
[INFO LINE2] 0.3:523 0.5:210 10:54 25:12 50:2 100:0 µm
```
Fields include:
- Line 1:
  - US/CN AQI Index
  - PM1.0 (CF=1 / Atm), PM2.5 (CF=1 / Atm), PM10 (CF=1 / Atm).
- Line 2:
  - Particle counts for 0.3–10 µm.

## Snapshots
![Snapshot](https://github.com/ilyanto/esp32_aqi/blob/main/snapshots.jpg)

## How to install
- Prerequisite: Must have a LittleFS partition.
- Compile the binary and flash it to the ESP32.
- Upload the file(s) in data folder to LittleFS partition: Ctrl+Shift+P > Upload LittleFS to ESP32.

## License
This project is licensed under the **MIT License**.

See the `LICENSE` file for details.

Email: ilyanto.radikiya@gmail.com for questions or more details.
