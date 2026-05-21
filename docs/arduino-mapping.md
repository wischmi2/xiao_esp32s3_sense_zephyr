# Arduino Reference → Zephyr Mapping

The [limengdu Arduino camera repo](https://github.com/limengdu/SeeedStudio-XIAO-ESP32S3-Sense-camera) is the primary hardware reference. This table maps its concepts to Zephyr equivalents.

## Camera

| Arduino / ESP-IDF | Zephyr |
|---|---|
| `esp_camera_init(&config)` | Video device init + `video_set_format()` |
| `esp_camera_fb_get()` | `video_enqueue()` / `video_dequeue()` |
| `esp_camera_fb_return()` | Return buffer to driver pool |
| `camera_config_t` pin fields | Devicetree `pinctrl` in Sense board + overlay |
| `PIXFORMAT_JPEG` | `VIDEO_PIX_FMT_JPEG` (when driver supports) |
| `FRAMESIZE_QVGA` | Width/height in `video_format` |
| `fb_count = 2` | Multiple buffers in video capture config |
| PSRAM enable in Arduino IDE | `CONFIG_ESP32S3_SPIRAM=y`, heap in PSRAM |
| `s->set_vflip(s, 1)` (OV3660) | OV3660 driver register write |
| `s->set_brightness(s, 1)` | OV3660 driver register write |
| `s->id.PID == OV3660_PID` | I2C probe + compatible `ovti,ov3660` |

### Camera GPIO (same on both stacks)

| GPIO | Signal |
|---|---|
| 10 | XMCLK |
| 11–18, 48 | DVP data |
| 13 | PCLK |
| 38 | VSYNC |
| 47 | HREF |
| 39 | SCL |
| 40 | SDA |

## Microphone

| Arduino | Zephyr |
|---|---|
| `#include <I2S.h>` / `ESP_I2S.h` | Zephyr I2S or DMIC subsystem |
| `I2S.setPinsPdmRx(42, 41)` | Devicetree: CLK=42, DATA=41 |
| `I2S.begin(PDM_MONO_MODE, 16000, 16)` | DMIC config: 16 kHz, 16-bit mono |
| `I2S.read()` | `dmic_read()` or I2S read API |
| PSRAM for WAV buffer | `k_heap` / SPIRAM allocation |

## microSD

| Arduino | Zephyr |
|---|---|
| `SD.begin(21)` | Sense DTS: SPI CS on GPIO 21 |
| `SD.open("/file.jpg", FILE_WRITE)` | Zephyr FS API: `fs_open()` on `/SD:` |
| SPI at 20–24 MHz | `spi-max-frequency` in devicetree |

## Wi-Fi Streaming

| Arduino | Zephyr |
|---|---|
| `WiFi.begin(ssid, pass)` | `wifi connect` shell or Wi-Fi API |
| `startCameraServer()` (HTTP MJPEG) | Custom socket server or HTTP server |
| ESP32 CameraWebServer example | Phase 5 app module `http_stream.c` |

## Files to Mine from Arduino Repo

| Path / topic | Use for |
|---|---|
| Camera pin definitions | Devicetree verification |
| OV3660 init / PID check | Phase 2 sensor driver port |
| JPEG capture to SD | Phase 2 validation pattern |
| WAV recording sketch | Phase 3 audio buffer sizing |
| Video record loop | Phase 5 frame timing reference |

## Upstream Zephyr Starting Points

| Sample | Phase |
|---|---|
| `samples/hello_world` | 0 |
| `samples/subsys/fs/fs_sample` | 1 |
| `samples/drivers/video/capture` | 2 |
| `samples/drivers/audio/dmic` | 3 |
| `samples/net/wifi` | 4 |
