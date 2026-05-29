# Lessons Learned

Issues encountered during XIAO ESP32S3 Sense bring-up on Windows, organized by phase.  
Update this file whenever a new blocker is resolved so future setup and later phases benefit.

**Related docs:** [docs/phases.md](docs/phases.md) · [ZEPHYR_PLAN.md](ZEPHYR_PLAN.md)

## Agent / workflow rule: confirm hardware before physical steps

**Before running `west flash`, `west espressif monitor`, or any step that needs the board, SD card, or antenna connected — ask the user to confirm hardware is hooked up.** Do not assume the XIAO is on USB, the Sense expansion is seated, or the SD card is inserted.

Typical checklist to confirm with the user:

- XIAO ESP32S3 Sense connected via **USB-C** (data port, not charge-only cable)
- Sense expansion board **seated** on the B2B connector (click)
- **microSD** inserted (Phases 1+)
- **Wi-Fi antenna** installed (Phases 4+)

Build-only steps (`west build`, `west update`, doc edits) do not require this confirmation.

**After any automated `west espressif monitor`:** run `.\scripts\kill-serial-monitor.ps1` so COM16 is not left locked for the user. See [docs/com-port-troubleshooting.md](docs/com-port-troubleshooting.md).

---

## Planning / Environment (pre-Phase 0)

### NCS workspace is not an ESP32 workspace

| Field | Detail |
|---|---|
| **Issue** | User already has NCS at `C:\ncs\` with West toolchains — that *is* a West workspace, but only for Nordic ARM targets. |
| **Symptom** | Attempting ESP32-S3 builds inside the NCS tree or with NCS toolchains fails or targets the wrong architecture. |
| **Fix** | Create a **separate** upstream Zephyr workspace at `C:\zephyrproject\` alongside `C:\ncs\`. Keep NCS unchanged for Nordic work. |
| **Notes** | NCS experience (West, Kconfig, devicetree) transfers; Xtensa toolchain and Espressif HAL blobs do not. |

### Upstream Sense board declares OV2640, hardware has OV3660

| Field | Detail |
|---|---|
| **Issue** | Upstream Zephyr Sense devicetree (`xiao_esp32s3_procpu_sense.dts`) declares **OV2640** (`ovti,ov2640`). This board ships with **OV3660**. |
| **Symptom** | Camera init may fail or produce wrong images if the wrong sensor driver is bound. |
| **Fix** | Deferred to **Phase 2** — port/adapt OV3660 driver or overlay; confirm I2C address on hardware. |
| **Notes** | Highest-risk item for the project. See [ZEPHYR_PLAN.md](ZEPHYR_PLAN.md) risk register. |

### Wrong board qualifier in early docs

| Field | Detail |
|---|---|
| **Issue** | Initial README and plan used `xiao_esp32s3/xiao_esp32s3_procpu/sense` as the build target. |
| **Symptom** | `west build -b xiao_esp32s3/xiao_esp32s3_procpu/sense` fails — board not found. |
| **Fix** | Use valid qualifiers on Zephyr **v4.1.0** (see Phase 0 below). Update all docs to match. |
| **Notes** | Board documentation on Zephyr main may differ from pinned LTS/release versions. Always verify with `west boards \| findstr xiao`. |

---

## Phase 0 — Toolchain and Blank Board

**Goal:** Flash `samples/hello_world` and see `Hello World! xiao_esp32s3` on serial.

### 1. Zephyr main vs SDK version mismatch

| Field | Detail |
|---|---|
| **Issue** | `west init --mr main` pulls Zephyr 4.4.99+, which requires `find_package(Zephyr-sdk 1.0)`. |
| **Symptom** | CMake error: user's **zephyr-sdk-0.17.2** (CMake package 0.17.2) is rejected at configure time. |
| **Fix** | Pin Zephyr to **v4.1.0** (compatible with SDK 0.17.x). Do **not** use `main` for this setup. |
| **Notes** | Init example: `west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.1.0` |

### 2. Wrong board target string

| Field | Detail |
|---|---|
| **Issue** | `-b xiao_esp32s3/xiao_esp32s3_procpu/sense` is not a valid board qualifier. |
| **Symptom** | West reports unknown board / build target not found. |
| **Fix** | On v4.1.0, valid targets are: |
| | • `xiao_esp32s3/esp32s3/procpu/sense` — Sense variant (camera, mic, SD) |
| | • `xiao_esp32s3/esp32s3/procpu` — non-Sense base board |
| **Notes** | Qualifier format changed between Zephyr versions; re-check after any version bump. |

### 3. Picolibc / SDK header conflict

| Field | Detail |
|---|---|
| **Issue** | Default toolchain Picolibc conflicts with SDK-bundled `sys/lock.h` when compiling Zephyr libc retarget hooks. |
| **Symptom** | Build fails in `locks.c` / `libc-hooks.c` with conflicting types for `k_mutex` retarget hooks. Reproduced on v4.0.0 and v4.1.0. |
| **Fix** | Enable module Picolibc via extra conf file: |
| | 1. Create `C:\zephyrproject\picolibc-module.conf` with `CONFIG_PICOLIBC_USE_MODULE=y` |
| | 2. Pass at build time: `-- -DEXTRA_CONF_FILE=C:/zephyrproject/picolibc-module.conf` |
| **Notes** | Required for every ESP32 build in this workspace until upstream/SDK alignment improves. |

### 4. Environment variables unset in fresh shells

| Field | Detail |
|---|---|
| **Issue** | `ZEPHYR_BASE` and `ZEPHYR_SDK_INSTALL_DIR` are empty in new PowerShell sessions. |
| **Symptom** | West/CMake cannot find Zephyr or the SDK; toolchain lookup fails. |
| **Fix** | Set before building: `ZEPHYR_SDK_INSTALL_DIR=C:\Users\Brian\zephyr-sdk-0.17.2` (adjust path if SDK lives elsewhere). Activate Zephyr environment per the [Windows getting started guide](https://docs.zephyrproject.org/latest/develop/getting_started/installation_win.html). |
| **Notes** | Use a dedicated ESP32 shell, not the nRF Connect / NCS environment. |

### 5. NCS ARM toolchain cannot compile ESP32-S3

| Field | Detail |
|---|---|
| **Issue** | Toolchains under `C:\ncs\` target ARM Cortex-M (Nordic), not Xtensa. |
| **Symptom** | No Xtensa compiler available; ESP32 builds fail at toolchain resolution. |
| **Fix** | Install [Zephyr SDK](https://github.com/zephyrproject-rtos/sdk-ng/releases) with **xtensa-espressif_esp32s3** support. Point `ZEPHYR_SDK_INSTALL_DIR` at that install. |
| **Notes** | `west blobs fetch hal_espressif` is also required in the ESP32 workspace. |

### 6. Long first-time `west update`

| Field | Detail |
|---|---|
| **Issue** | Initial workspace population downloads many modules and HAL blobs. |
| **Symptom** | `west update` runs 6+ minutes with little visible progress. |
| **Fix** | Wait for completion; run once per workspace. Subsequent updates are incremental. |
| **Notes** | Plan for network bandwidth and disk space (~several GB) on first setup. |

### 7. Flash attempted without confirming board was connected

| Field | Detail |
|---|---|
| **Issue** | Agent ran `west flash` / monitor without asking whether the XIAO was plugged in. |
| **Symptom** | Flash or monitor steps fail or produce ambiguous results (wrong COM port, no serial output) when hardware is absent or not ready. |
| **Fix** | **Ask the user first** whether the board is connected and ready. Only then run flash/monitor. |
| **Notes** | Documented as a standing workflow rule at the top of this file. |

### 8. Flash and serial monitor (hardware pending)

| Field | Detail |
|---|---|
| **Issue** | End-to-end flash + monitor not yet confirmed on physical hardware. |
| **Symptom** | First `west flash` failed: `esptool.py not found` under HAL path (may be timing/path before blobs fully present). Monitor opened COM4/COM5 but no `Hello World` without successful flash. |
| **Fix** | Confirm board is connected, then verify `C:\zephyrproject\modules\hal\espressif\tools\esptool_py\esptool.py` exists after `west blobs fetch hal_espressif`. Retry `west flash`; hold BOOT + tap RESET if needed. Use `west espressif monitor` on the XIAO USB-C port. |
| **Notes** | **Pass criterion:** serial prints `Hello World! xiao_esp32s3`. See [docs/phase0-setup.md](docs/phase0-setup.md). |

### 9. esptool.py missing at flash time

| Field | Detail |
|---|---|
| **Issue** | `west flash` requires `esptool.py` from the Espressif HAL module. |
| **Symptom** | `FATAL ERROR: required program .../esptool_py/esptool.py not found` |
| **Fix** | Run `west blobs fetch hal_espressif` from `C:\zephyrproject`. On Windows, also prepend `.PY` to `PATHEXT`: `$env:PATHEXT = ".PY;" + $env:PATHEXT` so Zephyr's `require()` finds `esptool.py`. If still missing, `pip install esptool`. |
| **Notes** | File should exist at `modules/hal/espressif/tools/esptool_py/esptool.py` after blobs fetch. Successful flash used **COM16** on this machine. |

### 10. Wrong COM port / busy port during flash or monitor

| Field | Detail |
|---|---|
| **Issue** | Multiple COM ports present; board may not be on the port esptool tries first; or port is permanently locked. |
| **Symptom** | `Write timeout`, `port is busy`, `Access is denied`, or `No serial data received` on COM3/COM4/COM5/COM16. |
| **Fix** | Identify the XIAO port in Device Manager. Close/kill other serial clients. See **[docs/com-port-troubleshooting.md](docs/com-port-troubleshooting.md)**. |
| **Notes** | After successful flash, monitor on the same port esptool used. |

### Phase 0 working build recipe (reference)

```powershell
cd C:\zephyrproject\zephyr
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
west build -p always -b xiao_esp32s3/esp32s3/procpu/sense samples/hello_world `
  -- -DEXTRA_CONF_FILE=C:/zephyrproject/picolibc-module.conf
west flash
west espressif monitor
```

---

## Phase 1 — SD Card + Filesystem

**Status:** Functionally complete (2026-05-21) — mount + read verified; strict log checklist hampered by dropped messages on full card.

### 1. SD storage init failure (legacy card / CMD8)

| Field | Detail |
|---|---|
| **Issue** | `fs_sample` builds and flashes, but SD card does not mount. |
| **Symptom** | Serial: `Card does not support CMD8, assuming legacy card` → `Storage init ERROR!` → `fs mount error (-5)` → `Error mounting disk.` |
| **Fix** | **Ensure microSD is fully seated** in the Sense expansion slot (gold contacts inward). Reseat if needed. Confirm FAT32, card ≤32 GB. |
| **Notes** | Root cause on 2026-05-21: card was **not inserted** in the board slot. After seating, mount and listing worked. |

### 2. Log messages dropped during large SD directory listing

| Field | Detail |
|---|---|
| **Issue** | Card with many existing files causes long `lsdir()` output at boot. |
| **Symptom** | `--- 26 messages dropped ---` in serial; `Block count`, `Disk mounted.`, `/SD:` header may not appear in capture. |
| **Fix** | Use an empty (or nearly empty) card for clean Phase 1 logs, or monitor longer / increase log buffer. Functional test still passes if `[FILE]` lines appear. |
| **Notes** | Observed listing IMAGE18–IMAGE37.JPG and VIDEO0.AVI from prior Arduino use. |

### 3. COM port busy during flash

| Field | Detail |
|---|---|
| **Issue** | `west flash` / monitor fail with `PermissionError: Access is denied` on COM16. |
| **Symptom** | Port locked by another serial monitor (Cursor, Arduino IDE, stale `idf_monitor`, etc.). Port often **stays locked** until the holding process is killed or USB is replugged. |
| **Fix** | Run **`.\scripts\kill-serial-monitor.ps1 -Port COM16`** — leftover `idf_monitor`/`python` from agent or automated `west espressif monitor` keeps the port open; **unplugging the XIAO does not release it**. Full guide: [docs/com-port-troubleshooting.md](docs/com-port-troubleshooting.md). |
| **Notes** | Primary cause in this project: agent/automated monitor not exiting cleanly. Kill PC process first, then retry flash. |

**Anticipated risks (unchanged):**

---

## Phase 2 — Camera Still Capture (OV3660)

**Status:** Complete (2026-05-29) — OV3660 driver, QVGA JPEG capture to `/SD:/ZEPHR000.JPG`.

### 1. OV3660 not visible on I2C without XMCLK

| Field | Detail |
|---|---|
| **Issue** | Initial I2C probe found no devices; SCCB read returned `-14` at 0x3c and 0x30. |
| **Symptom** | Empty I2C bus scan with only `CONFIG_I2C=y`. |
| **Fix** | Enable **`CONFIG_VIDEO_ESP32=y`** so `lcd_cam` starts **XMCLK (GPIO 10)** before SCCB probe. Wait ~100 ms after boot. |
| **Notes** | After fix: `0x3c: PID 0x3660 (OV3660)`. App: `app/cam_i2c_probe`. |

### 2. Upstream Sense DTS wrong sensor

| Field | Detail |
|---|---|
| **Issue** | `xiao_esp32s3_procpu_sense.dts` binds **OV2640 @ 0x30**; hardware is **OV3660 @ 0x3c**. |
| **Symptom** | OV2640 driver cannot probe sensor even with video stack enabled. |
| **Fix** | Port **OV3660 driver** (16-bit SCCB, like OV5640) + overlay `@3c`. |
| **Notes** | Out-of-tree module: `modules/ov3660/`. See [docs/phase2-ov3660.md](docs/phase2-ov3660.md). |

### 3. DMA error if SD write while capture running

| Field | Detail |
|---|---|
| **Issue** | Writing BMP to SD while `lcd_cam` still streaming exhausts video buffers. |
| **Symptom** | `Frame dropped. No buffer available`, `DMA error: -160` during/after capture. |
| **Fix** | Call **`video_stream_stop()`** immediately after `video_dequeue()`, before `fs_write()`. |
| **Notes** | App: `app/cam_capture_sd`. Fixed 2026-05-29. |

### 4. Deferred logging hid early boot output

| Field | Detail |
|---|---|
| **Issue** | `CONFIG_LOG_MODE_DEFERRED` — monitor showed only ESP-ROM/PSRAM lines for ~35 s. |
| **Symptom** | Appears hung after `SPI SRAM memory test OK`; no Zephyr banner. |
| **Fix** | Use **`CONFIG_LOG_MODE_IMMEDIATE=y`** for bring-up apps, or wait for log thread. |
| **Notes** | `config/cam-capture-sd-sense.conf`. |

### 5. `invert-byte-order` breaks JPEG capture

| Field | Detail |
|---|---|
| **Issue** | `invert-byte-order` on `lcd_cam` was added to fix RGB565 BMP color byte order. |
| **Symptom** | `JPEG SOI/EOI not found in 76800 byte buffer` — sensor outputs JPEG but DMA swaps byte pairs. |
| **Fix** | Remove `invert-byte-order` from overlay when using JPEG; JPEG colors are correct without it. |
| **Notes** | Log: `logs/serial/cam_jpeg_capture.txt`. Typical QVGA JPEG ~4–8 KB inside 76800-byte DMA buffer. |

**Remaining (optional):** Higher resolutions, RGB565 color path without psychedelic artifacts if BMP is needed again.

---

## Phase 3 — PDM Microphone

**Status:** Not started.

| Anticipated risk | Notes |
|---|---|
| **PDM overlay missing from upstream Sense DTS** | Must add I2S PDM devicetree overlay: GPIO 41 DATA, GPIO 42 CLK. |
| **I2S PDM binding support** | ESP32-S3 I2S driver exists; PDM/DMIC mode may need validation on v4.1.0. |
| **Sample rate lock-in** | Start at 16 kHz, 16-bit, mono — do not change until stable. |

**Notes:** _(none yet — add issues here as Phase 3 progresses)_

---

## Phase 4 — Wi-Fi Connectivity

**Status:** Not started.

| Anticipated risk | Notes |
|---|---|
| **Credentials management** | SSID/password via Kconfig or overlay — avoid committing secrets. |
| **Antenna required** | Install Wi-Fi antenna for reliable streaming in later phases. |
| **Network sample compatibility** | Start from `samples/net/wifi` with XIAO-specific overlay. |

**Notes:** _(none yet — add issues here as Phase 4 progresses)_

---

## Phase 5 — Camera Streaming over Wi-Fi

**Status:** Not started.

| Anticipated risk | Notes |
|---|---|
| **Wi-Fi + camera memory pressure** | Concurrent Wi-Fi stack and JPEG buffers may exhaust RAM/PSRAM — use QVGA, tune buffer count, single client. |
| **MJPEG throughput** | Realistic first target: QVGA ~5–10 FPS. |
| **HTTP server choice** | Custom sockets vs `CONFIG_HTTP_SERVER` — decide when integrating Phase 2 pipeline. |

**Notes:** _(none yet — add issues here as Phase 5 progresses)_

---

## Phase 6 — Scheduled OCR for Serial Numbers

**Status:** Not started.

| Anticipated risk | Notes |
|---|---|
| **OCR accuracy** | Wrong serial stored if confidence threshold too low — store image for audit. |
| **On-device vs backend OCR** | Tier B (POST JPEG to backend) recommended first; Tier A (TFLite Micro) if offline required. |
| **Trigger source undecided** | Time / HTTP POST / GPIO — pick one for v1 before implementation. |
| **Part serial format unknown** | Length, charset, barcode vs text affects ROI and validation regex. |

**Notes:** _(none yet — add issues here as Phase 6 progresses)_

---

## How to Add New Entries

When resolving a new issue:

1. Place it under the correct phase section (or Planning if pre-phase).
2. Use the **Issue / Symptom / Fix / Notes** table format.
3. For unresolved items, mark **Fix** as open or pending.
4. Cross-link to [docs/phases.md](docs/phases.md) checklist items when a phase pass criterion is affected.
