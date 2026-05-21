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
| **Issue** | Multiple COM ports present; board may not be on the port esptool tries first. |
| **Symptom** | `Write timeout`, `port is busy`, or `No serial data received` on COM3/COM4/COM5. |
| **Fix** | Identify the XIAO port in Device Manager. Use `west espressif monitor -p COM16` (adjust port). Close other serial monitors using the same port. |
| **Notes** | After successful flash, monitor on the same port that esptool used. |

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

**Status:** Not started.

| Anticipated risk | Notes |
|---|---|
| SD card format / size | Use FAT32, ≤32 GB for broad compatibility. |
| SPI pin mapping | Sense DTS wires SD via `spi2`; verify against hardware before custom overlays. |
| Mount path conventions | Zephyr FAT sample uses `/SD:` — confirm in `samples/subsys/fs/fs_sample`. |

**Notes:** _(none yet — add issues here as Phase 1 progresses)_

---

## Phase 2 — Camera Still Capture (OV3660)

**Status:** Not started.

| Anticipated risk | Notes |
|---|---|
| **No upstream OV3660 driver** | Sense DTS binds OV2640; must port/adapt driver or extend OV2640. Budget 1–2 weeks. |
| **I2C address uncertainty** | Scan bus — expect `0x3c` or document actual vs DTS `0x30`. |
| **PSRAM misconfiguration** | Enable `CONFIG_ESP32S3_SPIRAM`; frame alloc failures if PSRAM Kconfig wrong. |
| **GPIO 38 LED conflict** | Camera VSYNC shares user LED pin — disable LED in overlay when camera active. |
| **Sensor tuning** | vflip, brightness, saturation differ from OV2640; borrow from Arduino reference repo. |

**Notes:** _(none yet — add issues here as Phase 2 progresses)_

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
