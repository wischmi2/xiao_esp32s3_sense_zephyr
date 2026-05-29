# Phase 6 — GPIO Trigger and OCR

**Near-term (6a):** External GPIO signal triggers a JPEG capture to SD — no Wi-Fi required.  
**Long-term (6):** Same trigger path, plus ROI crop, OCR, and stored `{timestamp, serial, image_path}`.

Phases 4–5 (wireless) are **on hold** until a **2.4 GHz** network is available (ESP32-S3 does not support 5 GHz–only APs). GPIO trigger work can proceed entirely on Phase 2 hardware.

---

## Phase 6a — External GPIO Trigger

### Goal

An external signal (sensor, PLC via opto, dry contact, 3.3 V logic) causes the board to capture one QXGA JPEG and save it to microSD with an incrementing filename (`ZEPHR000.JPG`, …).

### Why this is feasible now

Phase 2 already implements GPIO-triggered capture via the **BOOT button on GPIO0** (`sw0` in `boards/xiao_esp32s3_sense_ov3660.overlay`). The app `app/cam_capture_sd`:

- Mounts SD and configures OV3660 for QXGA JPEG
- Runs an optional boot test capture
- Waits for **GPIO0 (BOOT)** press → capture → save

Phase 6a extends that pattern: same capture pipeline, different GPIO (and typically interrupt + debounce instead of simple button polling).

### Recommended free pins

With camera + microSD active, these **XIAO edge connector** pins are usually available:

| Pin | GPIO | Notes |
|-----|------|--------|
| **D0** | 1 | Good default for external trigger |
| **D1** | 2 | Good alternative |
| **D2–D5** | 3–6 | Available |
| **D6** | 43 | Available |
| **D7** | 44 | Available |

**Do not use for trigger:**

| Resource | GPIO | Reason |
|----------|------|--------|
| D8–D10 | 7–9 | microSD SPI (SCK, MISO, MOSI) |
| Camera DVP | 10–18, 38, 47, 48 | OV3660 data/sync |
| Camera I2C | 39, 40 | SCCB |
| PDM mic | 41, 42 | Sense microphone |
| SD CS / LED | 21 | SD chip select |
| BOOT | 0 | Already used as onboard trigger |

See [hardware.md](hardware.md) and [ZEPHYR_PLAN.md](../ZEPHYR_PLAN.md) for full pin tables.

### Signal wiring (3.3 V logic only)

ESP32-S3 GPIO is **3.3 V**. Do not connect 5 V or 24 V PLC outputs directly.

| Source | Typical wiring | Trigger style |
|--------|----------------|---------------|
| Dry contact / NPN open-collector | Switch between GPIO and GND; enable internal **pull-up** | **Active-low** (same as BOOT) |
| 3.3 V logic output | GPIO ← signal; common GND | Active-high or active-low (configurable) |
| PLC / 24 V | **Opto-isolator** or level shifter → 3.3 V → GPIO | Edge after isolation |

For production triggers (short pulses, bounce), prefer **GPIO interrupt** (rising or falling edge) with **10–50 ms debounce** instead of polling in a tight loop.

### Implementation plan (6a)

1. **Devicetree overlay** — add e.g. `trigger_input` on **D1 (GPIO2)** with pull-up, active-low; optional alias `trigger0`.
2. **Application** — extend `cam_capture_sd` (or fork `app/cam_trigger_sd`):
   - GPIO interrupt + debounce → semaphore
   - Main loop: wait for trigger → `capture_and_save()` (existing function)
   - Optional Kconfig: trigger pin, edge polarity, debounce ms, disable boot auto-capture
3. **Keep BOOT as backup** (optional) — useful for bench testing without external wiring.

### Pass criteria (6a)

- [ ] External signal (or jumper from D1 to GND) produces a new valid `.JPG` on `/SD:` every time
- [ ] No duplicate captures from bounce (debounce verified)
- [ ] Serial logs trigger event and saved filename
- [ ] QXGA capture still completes in ~5 s per shot (same as Phase 2)

### Open decisions (before coding)

| # | Question | Default if unspecified |
|---|----------|------------------------|
| 1 | Which pin? | **D1 (GPIO2)** |
| 2 | Signal type? | Dry contact / active-low with pull-up |
| 3 | Trigger edge? | **Falling** (active-low pulse or switch to GND) |
| 4 | Keep BOOT trigger? | Yes (both BOOT and external) |
| 5 | Boot auto-capture? | Disable for production; keep for bring-up |

---

## Phase 6 — OCR on Triggered Capture

### Goal

When Phase 6a fires a capture, optionally:

1. Crop a **ROI** (region of interest) for the serial/text area
2. Run **OCR** (Tier B: POST JPEG to backend; Tier A later: on-device TFLite)
3. **Validate** serial format (regex)
4. Store `{timestamp, serial, image_path}` to SD (CSV/JSON) or forward to a server

### Prerequisites

- Phase 6a stable and repeatable
- Fixture documented: distance, lighting, ROI — see [hardware.md](hardware.md)
- Serial number format defined (length, charset)

### OCR tiers

| Tier | Approach | Trade-off |
|------|----------|-----------|
| **B** | POST JPEG to backend OCR service | Better accuracy; needs network (when Wi-Fi returns) |
| **A** | TFLite Micro on-device | Offline; harder to tune |

Recommended: validate pipeline with Tier B when network is available; migrate to Tier A if offline is required.

### Pass criteria (6)

- [ ] Correct serial read from labeled test part ≥ N times (N TBD)
- [ ] Raw JPEG retained for audit when OCR fails or confidence is low

---

## Related files

| Path | Role |
|------|------|
| `app/cam_capture_sd/src/main.c` | Current BOOT-triggered capture (Phase 2 base for 6a) |
| `boards/xiao_esp32s3_sense_ov3660.overlay` | OV3660 + BOOT on GPIO0 |
| `config/cam-capture-sd-sense.conf` | PSRAM / DMA settings for QXGA |
| `scripts/build-cam-capture-sd.ps1` | Build and flash |

## References

- [phases.md](phases.md) — checklist and status
- [arduino-mapping.md](arduino-mapping.md) — Arduino → Zephyr mapping
- [Seeed pin multiplexing wiki](https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/)
