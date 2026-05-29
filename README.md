# xiao_esp32s3_sense_zephyr

Zephyr RTOS application for the [Seeed Studio XIAO ESP32S3 Sense](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) — OV3660 camera, PDM microphone, and microSD.

**Near-term goal:** Wi-Fi camera streaming (MJPEG).  
**Long-term goal:** Scheduled capture and OCR for part serial numbers.

Design plan: [ZEPHYR_PLAN.md](ZEPHYR_PLAN.md)  
Setup gotchas and resolved issues: [LESSONS_LEARNED.md](LESSONS_LEARNED.md)  
**COM port locked?** [docs/com-port-troubleshooting.md](docs/com-port-troubleshooting.md)

## Hardware

- Board: Seeed XIAO ESP32S3 **Sense** (OV3660, PDM mic, microSD slot)
- Build target: `xiao_esp32s3/esp32s3/procpu/sense`
- PSRAM required for camera workloads

## West Workspace Setup

### Already using NCS at `C:\ncs\`?

Yes — NCS is a West workspace, but it targets **Nordic ARM** chips. The XIAO ESP32S3 needs **upstream Zephyr + Xtensa toolchain + Espressif HAL**, so use a **separate** workspace. Keep `C:\ncs\` for Nordic work; do not mix ESP32 builds into the NCS tree.

### One-time: create the ESP32 workspace (Windows)

```powershell
# Create workspace (separate from C:\ncs\)
mkdir C:\zephyrproject\applications -Force
cd C:\zephyrproject
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.1.0
west update
west blobs fetch hal_espressif

# Clone this application
cd applications
git clone https://github.com/wischmi2/xiao_esp32s3_sense_zephyr.git
```

You also need the [Zephyr SDK](https://github.com/zephyrproject-rtos/sdk-ng/releases) **0.17.x** with **xtensa-espressif_esp32s3** support. The ARM toolchains under `C:\ncs\` cannot compile ESP32-S3 firmware. Set `ZEPHYR_SDK_INSTALL_DIR` (e.g. `C:\Users\Brian\zephyr-sdk-0.17.2`).

**Do not use `--mr main`** — Zephyr 4.4+ requires SDK 1.0. Pin **v4.1.0** for SDK 0.17.2 compatibility.

Full Phase 0 steps: [docs/phase0-setup.md](docs/phase0-setup.md)

### Phase 0 smoke test (upstream sample)

Before building this app, confirm the ESP32 workspace and board:

```powershell
# From repo root
.\scripts\build-hello.ps1 -Pristine
cd C:\zephyrproject\zephyr
west flash
west espressif monitor
```

Build requires Picolibc module workaround — see [docs/phase0-setup.md](docs/phase0-setup.md).

Expected: `Hello World! xiao_esp32s3`

### Windows notes

- Open a **new** terminal/shell for ESP32 work (`C:\zephyrproject\`), not the nRF Connect / NCS environment.
- Use `west espressif monitor` for serial console (USB-C on the XIAO).
- Put the board in bootloader mode (hold BOOT, tap RESET) if `west flash` fails.

## Repository Layout

```text
ZEPHYR_PLAN.md          # Architecture and phased plan
LESSONS_LEARNED.md      # Phase-organized setup issues and fixes
docs/
  phase0-setup.md       # Windows workspace + build + flash steps
  hardware.md           # Physical setup (OCR lighting, mounting)
  phases.md             # Phase checklists and progress tracking
  arduino-mapping.md    # Arduino reference → Zephyr equivalents
scripts/
  build-hello.ps1       # Phase 0 hello_world build helper
config/
  picolibc-module.conf  # Reference copy of Picolibc workaround Kconfig
app/                    # Zephyr application (added per phase)
```

## Development Phases

| Phase | Focus |
|---|---|
| 0 | Toolchain + hello world |
| 1 | SD card / FAT filesystem |
| 2 | OV3660 still capture |
| 3 | PDM microphone |
| 4 | Wi-Fi |
| 5 | MJPEG streaming |
| 6 | Scheduled OCR for serial numbers |

Details: [docs/phases.md](docs/phases.md)

## References

- Arduino reference (pin maps, OV3660 tuning): [limengdu/SeeedStudio-XIAO-ESP32S3-Sense-camera](https://github.com/limengdu/SeeedStudio-XIAO-ESP32S3-Sense-camera)
- Zephyr XIAO examples: [Cosmic-Bee/xiao-zephyr-examples](https://github.com/Cosmic-Bee/xiao-zephyr-examples)
