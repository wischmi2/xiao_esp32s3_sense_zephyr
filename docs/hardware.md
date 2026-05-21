# Hardware Setup

Physical configuration notes for the XIAO ESP32S3 Sense and OCR use case.

## Board Assembly

1. Install Wi-Fi/BT antenna (hook one side first, then press the other — do not force straight down).
2. Align Sense expansion board B2B connector; press until it clicks.
3. Insert microSD (gold contacts inward), FAT32 formatted, ≤32 GB.

## Pin Summary

See [ZEPHYR_PLAN.md](../ZEPHYR_PLAN.md) for full GPIO tables.

| Subsystem | Key pins |
|---|---|
| Camera DVP | GPIO 10–18, 38, 47, 48 |
| Camera I2C | GPIO 39 (SCL), GPIO 40 (SDA) |
| PDM mic | GPIO 41 (DATA), GPIO 42 (CLK) |
| microSD SPI | CS=21, SCK=7, MISO=8, MOSI=9 |

## OCR Physical Setup (Phase 6)

Document your fixture here as it is built:

| Parameter | Target | Actual |
|---|---|---|
| Camera-to-part distance | TBD | |
| Lighting type | Diffuse, minimal glare | |
| Text region in frame | ROI coordinates TBD | |
| Part presentation | Fixed jig / conveyor stop | |
| Serial format | TBD (see open questions in plan) | |

### Tips for readable captures

- Fixed distance and angle beat software tuning alone.
- Avoid direct reflections on stamped or laser-etched serials.
- ROI crop reduces OCR processing and false reads.
- Store raw JPEG alongside parsed serial for audit/retrain.

## Known Conflicts

- **GPIO 38:** DVP VSYNC and user LED — LED may be disabled when camera is active.
- **GPIO 41/42:** Mic vs optional D11/D12 header use — cutting J1/J2 on Sense board disables mic if repurposing those pins.

## References

- [Seeed pin multiplexing wiki](https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/)
- [Camera usage wiki](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/)
- [Microphone wiki](https://wiki.seeedstudio.com/xiao_esp32s3_sense_mic/)
