# Phase 5 — Wireless SD Gallery

**Goal:** Browse and download pictures stored on the microSD card from a browser on your LAN.

This replaces the original “live MJPEG stream” goal with **SD file serving**, which matches retrieving photos already captured by `cam_capture_sd` (e.g. `ZEPHR000.JPG` … `ZEPHR004.JPG`).

Live streaming can be added later as an optional `/stream` endpoint.

## Prerequisites

- Phase 4 Wi-Fi working (antenna, credentials)
- microSD inserted with JPEG files from Phase 2 captures
- PC/phone on the **same Wi-Fi network** as the XIAO

## Build and flash

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
$env:PATHEXT = ".PY;" + $env:PATHEXT
.\scripts\kill-serial-monitor.ps1 -Port COM16
.\scripts\build-sd-gallery.ps1 -Flash -Port COM16
west espressif monitor -p COM16
```

## Usage

1. Wait for serial: `>>> SD gallery at http://192.168.x.x/ <<<`
2. Open that URL in a browser.
3. Click any `.JPG` link to view or download the file.

### HTTP API

| Request | Response |
|---------|----------|
| `GET /` | HTML page listing `*.JPG` on `/SD:` |
| `GET /img/ZEPHR000.JPG` | JPEG file (streamed in 4 KB chunks) |

Only 8.3-style safe filenames are allowed (alphanumeric + `._-`, ends in `.JPG`).

## Pass criteria

- Browser shows a list of SD card JPEGs
- Clicking a file displays or downloads a valid image
- QXGA files (~2–3 MB) transfer without device reset

## Memory notes

Gallery mode does **not** load the full JPEG into RAM — it reads from SD in 4 KB chunks while sending over TCP. Camera buffers and PSRAM heap settings from Phase 2 are not required here.

## App

| Path | Role |
|------|------|
| `app/sd_gallery/` | Wi-Fi + SD mount + HTTP server |
| `config/sd-gallery-sense.conf` | SD + larger network buffers |

## Optional later

- `GET /stream` — live QVGA MJPEG (original Phase 5 plan)
- Combined firmware: capture on BOOT + serve gallery without reflashing
