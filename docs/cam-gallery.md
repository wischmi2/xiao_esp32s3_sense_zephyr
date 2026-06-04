# Combined Camera + Wi-Fi Gallery (`cam_gallery`)

**Goal:** One firmware for Phase 2 (capture) and Phase 5 (browser gallery) — no reflashing between shoot and browse.

| Action | How |
|--------|-----|
| **Capture** | Press **BOOT** → saves `/SD:/ZEPHRnnn.JPG` (1024×768 JPEG; full QXGA via `cam_capture_sd`) |
| **Browse** | Open `http://<device-ip>/` on the same Wi-Fi LAN |
| **Download** | Click a `.JPG` link (`GET /img/NAME.JPG`) |

## Prerequisites

- Phase 0 workspace + `wifi-credentials.conf`
- Wi-Fi antenna, microSD with FAT32
- OV3660 module in repo (`modules/ov3660`)

## Build and flash

```powershell
.\scripts\kill-serial-monitor.ps1 -Port COM13
.\scripts\build-cam-gallery.ps1 -Flash -Port COM13
west espressif monitor -p COM13
```

## Compared to separate apps

| App | Use when |
|-----|----------|
| `cam_capture_sd` | Camera bring-up only, no Wi-Fi |
| `sd_gallery` | Browse existing SD files only, no new captures |
| **`cam_gallery`** | **Daily use** — capture + gallery in one image |

## Notes

- DHCP IP changes (e.g. `192.168.4.55`); use the address printed on serial, not a fixed `.30`.
- Zephyr `heap_caps` routes through `k_malloc`; with camera enabled, raise `CONFIG_HEAP_MEM_POOL_SIZE` (see `config/cam-gallery-sense.conf`) or Wi-Fi init fails with `memory allocation failed`.
- Next shot number is chosen from existing `ZEPHR*.JPG` on the card (no overwrite unless you reuse a number).
- SD access is mutex-protected so HTTP downloads and BOOT capture do not corrupt FAT.
- Gallery HTML buffer is 4 KB — very long file lists may truncate; refresh after each capture.
- Large QXGA files (e.g. `ZEPHR000.JPG` ~2.4 MB) take tens of seconds to download over Wi-Fi; the index page stays up while the HTTP thread sends the file.
- If a download seemed to hang, **power-cycle or press RESET** and reload the gallery URL from serial (DHCP IP may change).
- No boot-time auto-capture (Wi-Fi and gallery start faster).
- Combined build uses **1024×768** (native half-QXGA — sharper than odd sizes like 800×600). For **2048×1536 QXGA**, use `cam_capture_sd`.
- **Blurry?** The lens on the camera module is **manual focus** — gently turn the lens barrel until a test shot looks sharp at your typical distance (30 cm–1 m). Firmware cannot fix out-of-focus optics.
- **Washed out / pink?** Firmware sets saturation −3, AE level −3 (darker in bright rooms), and sharpness +2. Take a **new** BOOT shot after each flash; old files keep old settings.
- Camera (`lcd_cam` / OV3660) initializes **after** Wi-Fi so both stacks fit in RAM.

## Pass criteria

- Serial: `>>> Gallery: http://192.168.x.x/  |  BOOT = capture <<<`
- BOOT saves a new `ZEPHRnnn.JPG`
- Browser lists the new file after refresh
