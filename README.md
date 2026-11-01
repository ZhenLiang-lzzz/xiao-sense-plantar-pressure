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

Open the repo root in PlatformIO (VS Code) and flash each board with its own environment, which sets its BLE name (`pressure1` to `pressure4`):

```bash
pio run -e pressure1 -t upload
```

The firmware samples at 50 Hz on its own thread and streams batches of binary samples over BLE. BLE always starts, USB or not; USB Serial prints text at a lower rate for debugging.

### Desktop app

```bash
cd app
conda env create -f environment.yml
conda activate xiao-pressure
python pressure_monitor.py
```

Connect each foot from its own panel over **Serial** (pick a COM port from the list) or **BLE** (scans for devices advertising as `Pressure-*`). From the top bar, use **Position Setup** to calibrate sensor placement on the foot outline (saved between runs), **Record** to log a session to CSV, and **Replay** to play a recorded session back through the same live views.

### Recording format

Each CSV row is one frame from one foot:

```
t,foot,seq,board_ms,S1,S2,S3,S4,S5,S6,ax,ay,az,gx,gy,gz
```

`t` is seconds since recording started, stamped by the PC when the BLE packet arrives, so frames in one packet share a value. `foot` is `L` or `R`, set by the panel the board is connected to. `seq` counts samples on the board (a jump means lost frames) and `board_ms` is the board's own clock. Both are blank for boards still on the old text firmware and over Serial. A foot that is not connected writes no rows. Replay also opens recordings in the two older formats.

To compare timing between feet, jump with both feet at the start and end of a session, then run:

```bash
python align_recording.py readout/recording_xxx.csv
```

This writes `recording_xxx_aligned.csv` with a `t_sync` column that puts both boards' clocks on one timeline, and reports lost frames and clock drift for each foot.

## License

No license has been added yet — all rights reserved by default until one is chosen.
