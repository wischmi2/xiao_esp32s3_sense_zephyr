# Development Phases

Track progress here. Each phase must pass before enabling the next in a combined build.

**Workflow:** Before `west flash`, monitor, or any step needing physical hardware, **confirm with the user** that the board (and SD card / antenna when relevant) is connected. See [LESSONS_LEARNED.md](../LESSONS_LEARNED.md).

**COM port locked (`Access is denied` on COM16)?** See [com-port-troubleshooting.md](com-port-troubleshooting.md).

Status key: `[ ]` not started · `[~]` in progress · `[x]` done

---

## Phase 0 — Toolchain and Blank Board

**Goal:** Flash and monitor on Windows using a **separate ESP32 workspace** (not `C:\ncs\`).

- [x] Confirm NCS at `C:\ncs\` stays for Nordic only (no changes needed there)
- [x] Create `C:\zephyrproject\` upstream West workspace (`west init --mr v4.1.0`, `west update`)
- [x] Install Zephyr SDK with **xtensa-espressif_esp32s3** (`C:\Users\Brian\zephyr-sdk-0.17.2`)
- [x] Set `ZEPHYR_SDK_INSTALL_DIR` in ESP32 build shell (not NCS shell)
- [x] `west blobs fetch hal_espressif`
- [x] Clone this repo to `C:\zephyrproject\applications\xiao_esp32s3_sense_zephyr`
- [x] Build `samples/hello_world` for `xiao_esp32s3/esp32s3/procpu/sense` (with Picolibc module conf)
- [x] `west flash` + `west espressif monitor` (COM16, 2026-05-21)

**Pass:** `Hello World! xiao_esp32s3` on serial.

**Notes:**

Phase 0 **complete**. Serial output: `Hello World! xiao_esp32s3/esp32s3/procpu/sense` on COM16 @ 115200. Windows flash required `$env:PATHEXT = ".PY;" + $env:PATHEXT` so `esptool.py` is found. See [docs/phase0-setup.md](phase0-setup.md) and [LESSONS_LEARNED.md](../LESSONS_LEARNED.md).

---

## Phase 1 — SD Card + Filesystem

**Goal:** Storage works independent of camera.

- [x] microSD inserted and seated (gold contacts inward)
- [x] Build `samples/subsys/fs/fs_sample` with Sense board
- [x] List directory on `/SD:` (IMAGE*.JPG, VIDEO0.AVI listed from card)
- [~] Write and read back a test file (`some.dat` likely created; boot log dropped 26 messages)

**Pass:** Test file visible on SD when read on PC.

**Notes:**

2026-05-21: **SD mount + read confirmed** after proper card insertion. Serial lists existing files from card (Arduino-era captures). Early boot lines (`Block count`, `Disk mounted.`) dropped due to log flood from large directory listing — use empty card or longer monitor for clean capture. See [LESSONS_LEARNED.md](../LESSONS_LEARNED.md).

## Phase 2 — Camera Still Capture (OV3660)

**Goal:** One JPEG frame saved to SD.

- [x] Confirm OV3660 I2C address — **0x3c**, PID **0x3660** (needs XMCLK)
- [x] Check upstream for `ovti,ov3660` — **none in v4.1.0**
- [x] Enable PSRAM Kconfig
- [x] Build from `samples/drivers/video/capture`
- [x] Port OV3660 driver (out-of-tree `modules/ov3660`)
- [x] Flash + verify frame capture on hardware (2026-05-29)
- [x] Apply OV3660 tuning (vflip, brightness, saturation) in `cam_capture_sd`
- [x] Capture QVGA JPEG to SD (`ZEPHR000.JPG`)
- [x] JPEG capture (OV3660 JPEG mode in driver)

**Pass:** Valid image on SD, viewable on PC. **Met** with JPEG (2026-05-29).

**Notes:**

2026-05-29: OV3660 confirmed via `app/cam_i2c_probe`. `app/cam_capture_sd` saves `/SD:/ZEPHR000.JPG`. Stop video stream before SD write to avoid DMA errors. Do not use `invert-byte-order` on `lcd_cam` for JPEG.

---

## Phase 3 — PDM Microphone

**Goal:** Live loudness over serial.

**Status:** Skipped for now (2026-05-21) — proceed to Phase 4/5 for wireless SD access.

- [ ] Add I2S PDM devicetree overlay (GPIO 41 DATA, GPIO 42 CLK)
- [ ] 16 kHz, 16-bit, mono — do not change until stable
- [ ] Print RMS or raw samples to serial plotter

**Pass:** Plot responds when speaking near mic.

**Notes:**

Deferred. Can revisit after Phase 5 if audio streaming is needed.

---

## Phase 4 — Wi-Fi Connectivity

**Goal:** Join AP and reach device from LAN.

**Status:** On hold (2026-05-29) — no usable **2.4 GHz** AP at current location (ESP32-S3 cannot join 5 GHz–only networks).

- [~] Build Wi-Fi app with XIAO overlay (`app/wifi_connect`)
- [~] Configure SSID/password (`config/wifi-credentials.conf`)
- [ ] Flash + verify DHCP IP on serial
- [ ] Ping device IP from PC

**Pass:** Stable connection with antenna installed.

**Notes:**

See [phase4-wifi.md](phase4-wifi.md). Build: `.\scripts\build-wifi-connect.ps1 -Flash`

---

## Phase 5 — Wireless SD Gallery

**Goal:** Browse and download JPEG files from microSD over Wi-Fi (adapted from original MJPEG stream plan).

**Status:** On hold — depends on Phase 4 (2.4 GHz Wi-Fi).

- [~] HTTP server on port 80 (`app/sd_gallery`)
- [~] `GET /` — HTML listing of `*.JPG` on `/SD:`
- [~] `GET /img/NAME.JPG` — download file from SD
- [ ] Flash + verify in browser on LAN

**Pass:** Browser lists and displays SD card photos. QXGA files download successfully.

**Notes:**

See [phase5-sd-gallery.md](phase5-sd-gallery.md). Build: `.\scripts\build-sd-gallery.ps1 -Flash`

Optional later: live MJPEG `/stream` endpoint.

---

## Phase 6a — External GPIO Trigger

**Goal:** External signal triggers QXGA JPEG capture to SD (extends Phase 2 BOOT flow).

- [ ] Choose trigger pin (default: **D1 / GPIO2**)
- [ ] Devicetree overlay for external trigger input
- [ ] GPIO interrupt + debounce in `cam_capture_sd` (or dedicated app)
- [ ] Verify capture on each trigger edge

**Pass:** Each valid external trigger produces a new `.JPG` on `/SD:` without bounce duplicates.

**Notes:**

See [phase6-gpio-trigger.md](phase6-gpio-trigger.md). Builds on `app/cam_capture_sd`. No Wi-Fi required.

---

## Phase 6 — OCR on Triggered Capture

**Goal:** Triggered capture → serial string stored.

- [ ] Phase 6a trigger stable
- [ ] ROI crop for text region
- [ ] OCR Tier B: POST JPEG to backend service
- [ ] Validate serial format (regex)
- [ ] Store `{timestamp, serial, image_path}` to SD or server
- [ ] Optional: migrate to on-device TFLite (Tier A)

**Pass:** Correct serial read from labeled test part ≥N times.

**Notes:**

Full design: [phase6-gpio-trigger.md](phase6-gpio-trigger.md). Trigger options were originally time / HTTP / GPIO — **GPIO first** (6a), OCR second (6).
