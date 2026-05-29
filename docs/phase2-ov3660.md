# Phase 2 — OV3660 camera on Zephyr

## Status

| Step | Description | Status |
|---|---|---|
| 2.1 | I2C probe — confirm sensor PID at `0x3c` | **Done** — PID 0x3660 |
| 2.2 | Video capture sample build (lcd_cam + PSRAM) | **Done** — builds with OV3660 module |
| 2.3 | OV3660 Zephyr driver (out-of-tree module) | **Working** — QVGA RGB565 + JPEG on hardware |
| 2.4 | QVGA capture to SD (`ZEPHR000.JPG`) | **Done** — ~4.8 KB JPEG on `/SD:` |

## Hardware facts (OV3660 on XIAO Sense)

| Item | Value |
|---|---|
| SCCB I2C address | **0x3c** (not `0x30` in upstream Sense DTS) |
| Chip ID registers | 16-bit: `0x300A` / `0x300B` |
| Expected PID | **0x3660** |
| Control bus | `i2c1` — GPIO 39 SCL, GPIO 40 SDA |
| Data bus | DVP via `lcd_cam` (14 GPIOs — see ZEPHYR_PLAN.md) |

Upstream Zephyr v4.1.0 has **OV2640** in `xiao_esp32s3_procpu_sense.dts` at `0x30` and **no `ovti,ov3660` driver**. The OV3660 uses **16-bit SCCB** registers (like OV5640), not the OV2640 8-bit model.

## Step 2.1 — I2C probe

**Note:** The OV3660 SCCB bus does not respond until **XMCLK** (GPIO 10) is running. The probe app enables `CONFIG_VIDEO_ESP32` so `lcd_cam` starts the master clock at boot.

```powershell
cd C:\Users\Brian\GitHub\xiao_esp32s3_sense_zephyr
.\scripts\kill-serial-monitor.ps1 -Port COM16
.\scripts\build-cam-probe.ps1 -Flash -Port COM16
cd C:\zephyrproject\zephyr
west espressif monitor -p COM16
```

Expected output:

```text
  0x3c: PID 0x3660 (OV3660)
```

Press **`Ctrl+]`** when done, then run `kill-serial-monitor.ps1` if the port stays locked.

## Step 2.2 — Video capture build

Build succeeded (2026-05-29) with out-of-tree OV3660 module + Sense overlay:

```powershell
.\scripts\build-video-capture.ps1 -Pristine
```

Flash (confirm XIAO connected on COM16 first):

```powershell
.\scripts\kill-serial-monitor.ps1 -Port COM16
.\scripts\build-video-capture.ps1 -Flash -Port COM16
cd C:\zephyrproject\zephyr
west espressif monitor -p COM16
```

Expected serial: `OV3660 detected (PID 0x3660)`, video format `RGBP 320x240`, `Capture started`.

**Verified 2026-05-29:** Flash + monitor on COM16 — sensor init OK, lcd_cam capture started, no DMA/buffer errors. Log saved to `serial_video_capture.txt`.

## Step 2.4 — Save QVGA frame to microSD

App: `app/cam_capture_sd` — one JPEG frame → `/SD:/ZEPHR000.JPG`.

```powershell
.\scripts\kill-serial-monitor.ps1 -Port COM16
.\scripts\build-cam-capture-sd.ps1 -Flash -Port COM16
.\scripts\capture-serial.ps1 -Port COM16 -OutFile cam_jpeg_capture.txt
```

Expected serial:

```text
SD mounted at /SD:
Warmup frame: 76800 bytes (discarded)
Frame captured: 76800 bytes
Wrote /SD:/ZEPHR000.JPG (4780 bytes JPEG from 76800 byte buffer)
Done — open /SD:/ZEPHR000.JPG on PC
```

**Important:** Stop the video stream before writing to SD (DMA errors if lcd_cam keeps running during a long fs write).

**JPEG notes:**

- Do **not** set `invert-byte-order` on `lcd_cam` for JPEG — byte swapping breaks `FF D8` / `FF D9` markers (RGB565 BMP needed it for color fix).
- Discard one warmup frame before saving; the OV3660 JPEG pipeline often needs it.
- Serial logs go under `logs/serial/` (see `scripts/capture-serial.ps1`).

**Verified 2026-05-29:** JPEG capture pass — log `logs/serial/cam_jpeg_capture.txt`. Earlier BMP validation used `ZEPHR000.BMP` (153600-byte RGB565 frame, wrong colors without byte swap).

## Step 2.3 — OV3660 driver (in repo)

Out-of-tree module at `modules/ov3660/`:

- `ovti,ov3660` binding + `boards/xiao_esp32s3_sense_ov3660.overlay` (sensor `@3c`)
- Driver adapted from OV5640 SCCB + esp32-camera init tables
- QVGA RGB565 and JPEG; controls: vflip, hmirror, brightness, saturation, JPEG quality

Port path for remaining work:

1. ~~Adapt OV5640 16-bit SCCB~~ — done in `modules/ov3660/drivers/video/ov3660.c`
2. Expand init / JPEG from [esp32-camera ov3660.c](https://github.com/espressif/esp32-camera/blob/master/sensors/ov3660.c) — **JPEG done**
3. Apply Arduino tuning: vflip, brightness, saturation — **partial** (vflip/brightness/saturation in driver)

ESP-IDF / Arduino requirements for OV3660 on this board:

- **OCTAL PSRAM** (`CONFIG_SPIRAM_MODE_OCT=y`)
- **2 frame buffers** minimum
- QVGA first for stability

## References

- [limengdu Arduino camera repo](https://github.com/limengdu/SeeedStudio-XIAO-ESP32S3-Sense-camera)
- [manjotkhangura ESP32S3-Sense-OV3660](https://github.com/manjotkhangura/ESP32S3-Sense-OV3660) (ESP-IDF, OV3660-specific PSRAM notes)
- Upstream Sense DTS: `boards/seeed/xiao_esp32s3/xiao_esp32s3_procpu_sense.dts`
