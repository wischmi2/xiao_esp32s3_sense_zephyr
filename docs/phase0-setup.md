# Phase 0 Setup (Windows)

Verified setup for Seeed XIAO ESP32S3 Sense with **Zephyr v4.1.0** and **Zephyr SDK 0.17.2**, separate from NCS at `C:\ncs\`.

## Prerequisites

| Item | Path / value |
|---|---|
| NCS (Nordic only) | `C:\ncs\` — do not modify |
| ESP32 workspace | `C:\zephyrproject\` |
| Zephyr SDK | `C:\Users\Brian\zephyr-sdk-0.17.2` |
| west | `C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe` |
| Board target | `xiao_esp32s3/esp32s3/procpu/sense` |

## One-time workspace setup

```powershell
mkdir C:\zephyrproject\applications -Force
cd C:\zephyrproject
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.1.0
west update
west blobs fetch hal_espressif

cd applications
git clone https://github.com/wischmi2/xiao_esp32s3_sense_zephyr.git
```

**Do not use `--mr main`.** Zephyr main (4.4+) requires Zephyr SDK 1.0 CMake package; SDK 0.17.2 is rejected.

## Picolibc workaround

Create `C:\zephyrproject\picolibc-module.conf`:

```kconfig
CONFIG_PICOLIBC_USE_MODULE=y
```

Required for ESP32 builds with SDK 0.17.2 on v4.1.0 (toolchain Picolibc header conflict). See [LESSONS_LEARNED.md](../LESSONS_LEARNED.md).

## Build hello_world

From repo root:

```powershell
.\scripts\build-hello.ps1
```

Or manually:

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
cd C:\zephyrproject\zephyr
west build -p always -b xiao_esp32s3/esp32s3/procpu/sense samples/hello_world `
  -- -DEXTRA_CONF_FILE=C:/zephyrproject/picolibc-module.conf
```

Output: `C:\zephyrproject\zephyr\build\zephyr\zephyr.elf`

## Flash and monitor

**Before flashing:** confirm the XIAO ESP32S3 Sense is connected via USB-C and the Sense expansion board is seated. Do not run `west flash` until the user confirms hardware is ready.

Use a **dedicated ESP32 shell** (not nRF Connect / NCS environment).

On Windows, before `west flash`, set:

```powershell
$env:PATHEXT = ".PY;" + $env:PATHEXT
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
```

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
cd C:\zephyrproject\zephyr
west flash
west espressif monitor
```

If flash fails:

1. Hold **BOOT**, tap **RESET**, release BOOT (bootloader mode), retry `west flash`
2. Confirm `esptool.py` exists: `C:\zephyrproject\modules\hal\espressif\tools\esptool_py\esptool.py`
3. Install esptool if missing: `pip install esptool`

**Pass criterion:** serial prints `Hello World! xiao_esp32s3`

## Verify board target

```powershell
cd C:\zephyrproject\zephyr
west boards | findstr xiao
```

Expected Sense target: `xiao_esp32s3/esp32s3/procpu/sense`
