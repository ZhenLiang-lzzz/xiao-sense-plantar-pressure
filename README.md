# XIAO Sense Plantar Pressure

Firmware and desktop app for a 6-channel plantar pressure insole built around the **Seeed XIAO nRF52840 Sense**. The board's 6 analog inputs read external force sensors and its onboard **LSM6DS3 IMU** (accelerometer + gyroscope) is streamed alongside them over USB Serial or BLE (Nordic UART Service) to a PyQt6 desktop monitor.

## Hardware

- Custom carrier PCB for the Seeed XIAO nRF52840 Sense — schematic, BOM, Gerbers and pick-and-place files in [`hardware/`](hardware/)
- 6x external force/pressure sensors on analog inputs A0–A5
- Onboard LSM6DS3 IMU, read over I2C
- Streams over USB Serial (115200 baud) when connected to a host, falls back to BLE advertising otherwise

## Repository layout

| Path | Contents |
|---|---|
| `src/`, `include/`, `lib/`, `platformio.ini` | XIAO nRF52840 Sense firmware (PlatformIO, Arduino framework) |
| `hardware/` | Carrier board schematic, BOM, Gerbers, pick-and-place |
| `app/` | PyQt6 desktop monitor (`pressure_monitor.py`) — live dual-foot heatmap, pressure/IMU trend charts, sensor position calibration, CSV recording and replay |

## Getting started

### Firmware

Open the repo root in PlatformIO (VS Code) and flash the board:

```bash
pio run -t upload
```

### Desktop app

```bash
cd app
conda env create -f environment.yml
conda activate xiao-pressure
python pressure_monitor.py
```

Connect over **Serial** (auto-detected COM port) or **BLE** (scans for devices advertising as `Pressure-*`) from the app's top bar, then use **Position Setup** to calibrate sensor placement on the foot outline, **Record** to log a session to CSV, and **Replay** to play a recorded session back through the same live views.

## License

No license has been added yet — all rights reserved by default until one is chosen.
