# Development Phases

Track progress here. Each phase must pass before enabling the next in a combined build.

Status key: `[ ]` not started · `[~]` in progress · `[x]` done

---

## Phase 0 — Toolchain and Blank Board

**Goal:** Flash and monitor on Windows using a **separate ESP32 workspace** (not `C:\ncs\`).

- [ ] Confirm NCS at `C:\ncs\` stays for Nordic only (no changes needed there)
- [ ] Create `C:\zephyrproject\` upstream West workspace (`west init`, `west update`)
- [ ] Install Zephyr SDK with **xtensa-espressif_esp32s3** (ARM toolchains in `C:\ncs\` are not sufficient)
- [ ] Set `ZEPHYR_SDK_INSTALL_DIR` and activate ESP32 Zephyr environment (new shell, not NCS shell)
- [ ] `west blobs fetch hal_espressif`
- [ ] Clone this repo to `C:\zephyrproject\applications\xiao_esp32s3_sense_zephyr`
- [ ] Build `samples/hello_world` for `xiao_esp32s3/xiao_esp32s3_procpu/sense`
- [ ] `west flash` + `west espressif monitor`

**Pass:** `Hello World! xiao_esp32s3` on serial.

**Notes:**

See [LESSONS_LEARNED.md](../LESSONS_LEARNED.md) for resolved Phase 0 blockers (Zephyr version pin, board qualifier, Picolibc conf, env vars). Flash/monitor on hardware still pending confirmation.

---

## Phase 1 — SD Card + Filesystem

**Goal:** Storage works independent of camera.

- [ ] Format microSD FAT32 (≤32 GB)
- [ ] Build `samples/subsys/fs/fs_sample` with Sense board
- [ ] List directory on `/SD:`
- [ ] Write and read back a test file

**Pass:** Test file visible on SD when read on PC.

**Notes:**

---

## Phase 2 — Camera Still Capture (OV3660)

**Goal:** One JPEG frame saved to SD.

- [ ] Confirm OV3660 I2C address (scan: expect `0x3c` or document actual)
- [ ] Check upstream Zephyr for `ovti,ov3660` binding on chosen version
- [ ] Enable PSRAM Kconfig
- [ ] Build from `samples/drivers/video/capture`
- [ ] Apply OV3660 tuning (vflip, brightness, saturation from Arduino ref)
- [ ] Capture QVGA JPEG to SD

**Pass:** Valid `.jpg` on SD, opens correctly on PC.

**Notes:**

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
