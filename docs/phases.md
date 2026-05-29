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

- [ ] Add I2S PDM devicetree overlay (GPIO 41 DATA, GPIO 42 CLK)
- [ ] 16 kHz, 16-bit, mono — do not change until stable
- [ ] Print RMS or raw samples to serial plotter

**Pass:** Plot responds when speaking near mic.

**Notes:**

---

## Phase 4 — Wi-Fi Connectivity

**Goal:** Join AP and reach device from LAN.

- [ ] Build `samples/net/wifi` with XIAO overlay
- [ ] Configure SSID/password
- [ ] `wifi scan` + connect
- [ ] Ping device IP from PC

**Pass:** Stable connection with antenna installed.

**Notes:**

---

## Phase 5 — Camera Streaming over Wi-Fi

**Goal:** MJPEG stream viewable in browser.

- [ ] HTTP server on port 80
- [ ] `GET /stream` multipart MJPEG from Phase 2 pipeline
- [ ] Optional: `GET /` simple status page with IP

**Pass:** Browser shows live stream on LAN (QVGA ~5–10 FPS acceptable).

**Notes:**

---

## Phase 6 — Scheduled OCR for Serial Numbers

**Goal:** Triggered capture → serial string stored.

- [ ] Choose trigger: time / HTTP POST / GPIO
- [ ] ROI crop for text region
- [ ] OCR Tier B: POST JPEG to backend service
- [ ] Validate serial format (regex)
- [ ] Store `{timestamp, serial, image_path}` to SD or server
- [ ] Optional: migrate to on-device TFLite (Tier A)

**Pass:** Correct serial read from labeled test part ≥N times.

**Notes:**
