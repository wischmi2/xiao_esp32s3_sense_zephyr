# Phase 4 — Wi-Fi Connectivity

**Goal:** Join your home AP and reach the XIAO from the LAN.

## Prerequisites

- Wi-Fi antenna attached to the XIAO ESP32S3 Sense
- Board on USB (COM16 or your port)
- 2.4 GHz Wi-Fi AP (ESP32-S3 does not support 5 GHz-only networks)

## Setup credentials

```powershell
cd C:\Users\Brian\GitHub\xiao_esp32s3_sense_zephyr
copy config\wifi-credentials.conf.example config\wifi-credentials.conf
# Edit wifi-credentials.conf — set CONFIG_WIFI_CREDENTIALS_STATIC_SSID and _PASSWORD
```

`wifi-credentials.conf` is gitignored.

## Build and flash

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
$env:PATHEXT = ".PY;" + $env:PATHEXT
.\scripts\kill-serial-monitor.ps1 -Port COM16
.\scripts\build-wifi-connect.ps1 -Flash -Port COM16
west espressif monitor -p COM16
```

## Pass criteria

Serial shows:

```text
>>> Wi-Fi ready — ping 192.168.x.x <<<
```

From your PC on the same LAN:

```powershell
ping 192.168.x.x
```

## Fallback: shell commands

If credentials are missing or wrong, use the Zephyr shell over serial:

```text
uart:~$ wifi scan
uart:~$ wifi connect "YourSSID" 4 YourPassword
```

Security type `4` = WPA2-PSK.

## App

| Path | Role |
|------|------|
| `app/wifi_connect/` | Auto-connect + print IP |
| `boards/xiao_esp32s3_sense_wifi.overlay` | Enables `&wifi` |
| `config/wifi-sense.conf` | Network + shell Kconfig |

## Next

Phase 5 — [phase5-sd-gallery.md](phase5-sd-gallery.md): browse JPEG files on the SD card in a browser.
