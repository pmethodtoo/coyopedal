---
name: bringup-waveshare-lcd2
description: Use when building, flashing or recovering firmware for the Waveshare ESP32-S3-Touch-LCD-2 (`--board lcd-2`) fork, when a fresh clone needs the board fork reinstalled, when the screen is blank or snow, when the board will not enter download mode, or when anything in the vendored @geastack/targets fork is touched.
---

# Waveshare ESP32-S3-Touch-LCD-2 bring-up (lcd-2 fork)

This board is **not a published Geastack target**. Everything that makes the
pedalboard run on it lives in a fork cut into `@geastack/targets` at
**0.1.87**, plus repo-side overrides in `package.json` / `src/native`. The
fork's source of truth is the committed baseline at
`third_party/geastack-targets/` — `npm ci` wipes node_modules, and the fork
is **not reconstructible from memory**. Never rebuild it by hand; install it:

```bash
npm ci
npm run install:lcd2-fork
```

Then treat everything under `node_modules/@geastack/targets/targets/esp32-s3-waveshare-touch-lcd-2/`
as frozen data. The repo's own rules (AGENTS.md: board layer comes from
published packages) are deliberately overridden here — this is a permanent fork
by user decision, not a pending upstream contribution.

## Known-good state (what "working" means here)

As of 2026-10-08: full pedalboard UI paints, **touch works** (mapping fixed),
**USB audio works** (host mode), the audio interface enumerates. Known open
bug: **tuner crashes** to maintenance mode — do not "fix" it by rewriting the
display/touch stack. The nightly PSRAM 40 MHz downclock and related sdkconfig
experiments that came after are **not part of the working state** and have
been reverted out of `src/native/sdkconfig.defaults`.

Provenance of every piece, so nobody re-derives it:

| Piece                                                                                                      | Lives in                                                               |
| ---------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Forked target (ST7789T3 SPI panel, CST816D touch, pins, link flags, staging bands, PSRAM panel-push stack) | `third_party/geastack-targets/targets/esp32-s3-waveshare-touch-lcd-2/` |
| `gea_framework.cmake` quoted-board-define + filter fixes                                                   | `third_party/geastack-targets/patches/` (applied by the installer)     |
| targets.json registration (`ipcTaskStackSize: 4096`)                                                       | merged by the installer                                                |
| Partition table + board defines + CSS DPR 0.75                                                             | `package.json` → `gea.targets.esp32`                                   |
| Main task stack 8192, USB-host debug park                                                                  | `src/native/sdkconfig.defaults`, `src/native/drivers/usb_audio.cpp`    |
| IDF `build.cmake` quote-escape fix                                                                         | `~/esp/esp-idf` (patch in `patches/`, applied by the installer)        |
| Download-mode pulse, flash offsets, log-catching playbook                                                  | `mydocs/ESP32-LCD2-FLASH.md`, `tools/esp32/lcd2_download_mode.py`      |
| Board facts, docs map, session logs                                                                        | `mydocs/ESP32-DOCS.md`                                                 |

## Board facts that break builds if "tidied"

- **I2C is SDA=48, SCL=47.** They were once swapped in `board.h` and every
  device NACKed. The boot log must read `i2c: I2C bus ready (SDA=48, SCL=47)`
  and `CST816D present, version=0xB6` — those two lines are the health check.
- Touch mapping: CST816D reports portrait, panel flushes landscape —
  `lx = 319 - ty, ly = tx` in the fork's `touch.cpp`. Taps landing in the
  wrong place means this map (or the flush direction) changed.
- LCD SPI: MOSI=38 SCLK=39 CS=45 DC=42 BL=1, RST not connected; TF card
  shares the bus (CS=41). Display is 240×320 native / 320×240 logical.
- Staging flush bands are **8 rows** (≈3.8 KB); 32 rows (≈15 KB) fails
  after the NAM model arena claims contiguous DMA RAM
  (`display staging band alloc ... failed` → blank screen). Do not raise it.
- The panel push task stack **must** be in PSRAM
  (`xTaskCreatePinnedToCoreWithCaps` + `freertos/idf_additions.h`); internal
  core-1 RAM is starved by audio arenas (`failed to start panel push task`).
- The es8311 codec entry in the headless `.gea/targets` JSON is a **fake** —
  the board has no codec; the upstream base cmake forces the binding to
  compile unless pins are declared. Do not "clean up" the placeholder.
- `GEA_BOARD_HAS_POWER=0` and the quoted `GEA_EMBEDDED_CPP_BOARD` patch exist
  to kill `-Werror` redefinition collisions. Both are load-bearing.

## Build + flash, end to end

```bash
npm ci
npm run install:lcd2-fork
npm run build:firmware:lcd2 # gea build --board lcd-2
npm run flash:firmware:lcd2 # gea flash --board lcd-2 (background it)
```

If you must bypass `gea flash`, the offsets for this board (16 MB layout, two
4 MB OTA slots — see `mydocs/ESP32-LCD2-FLASH.md` for the full playbook and
why the single USB-C port is momentary by design):

```bash
. "$HOME/esp/esp-idf/export.sh"
B=.gea/build/esp32-s3-waveshare-touch-lcd-2/app-builds/nam-pedalboard
python3 -m esptool --chip esp32s3 -p /dev/cu.usbmodemXXXX -b 460800 write_flash \
  0x0 "$B/bootloader/bootloader.bin" 0x8000 "$B/partition_table/partition-table.bin" \
  0xf000 "$B/ota_data_initial.bin" 0x20000 build/pedalboard.bin
python3 -m esptool --chip esp32s3 -p /dev/cu.usbmodemXXXX run
```

`Wrong boot mode detected (0x4)` = firmware is running; retry in a loop, the
console window opens ~1 in 10 boots. The reliable software route is
`python3 tools/esp32/lcd2_download_mode.py "$p"` (DTR/RTS pulse, TRM ch. 33)
— it beats every button dance on this board. Do not unplug it to "try
buttons" first. Remove any USB audio interface from the host port before
flashing attempts.

## Verification markers (grep the boot log)

```
i2c: I2C bus ready (SDA=48, SCL=47)
CST816D present, version=0xB6
ST7789 SPI panel ready
```

Screen snow at init then UI = staging-band or half-flash; full blank with a
live console port = stuck in download mode, do `esptool run`.

## Holes to not fall into

- **Do not run `npm update` or install `@geastack/cli@latest`** — registry
  latest (0.1.97) requires a never-published targets 0.1.97. Pins: cli
  0.1.89 / targets 0.1.87, and the fork only cuts against 0.1.87.
- **Do not "restore" board code by rewriting it.** Diff against
  `third_party/geastack-targets/` instead; that tree is the answer.
- Never call `idf_build_set_property(COMPILE_DEFINITIONS ...)` with a full
  SET from the fork's top-level CMakeLists (corrupts IDF's
  `build_properties.temp.cmake` round-trip); the define-collision fix is the
  cache-list rewrite already in the forked `CMakeLists.txt`.
- `GEA_LCD2_DEBUG_CONSOLE` (park block in `usb_audio.cpp`) kills USB host for
  console bring-up. It must stay **off** for working-audio builds; only flip
  it when the console must survive boot.
- Wi-Fi creds live only in the gitignored
  `src/native/services/remote_config.h` (`npm run remote:configure`).
- Flash artifacts land in `build/` and `.gea/build/.../nam-pedalboard/`;
  `managed_components/` is regenerated — never commit or vendor it.

## Headless devkit spin-off (esp32-s3-pedal-headless)

The bare N16R8 devkit (UART bridge, no panel) runs the pristine repo brain
with zero fork code — but the published `esp32-s3-devkit-n16r8` target hits
the same upstream gaps as the LCD-2 (`GEA_BOARD_HAS_POWER` collision and the
es8311 binding compiled for a codec-less board). The fix lives in gitignored
`.gea/`, so it was stranded once already; the definitions are mirrored to
`tools/esp32/board-defs/`. On a fresh clone:

```bash
cp tools/esp32/board-defs/esp32-s3-pedal-headless.json .gea/targets/
# register the alias in .gea/boards.json (see boards.json.example):
#   "s3-devkit": { "target": "esp32-s3-pedal-headless",
#                  "targetDefinition": "targets/esp32-s3-pedal-headless.json",
#                  "appPlatform": "esp32" }
npm run build:firmware:s3-devkit   # gea build --board s3-devkit
```

Flash over the UART bridge (auto-reset works, unlike the LCD-2):
`esptool write_flash 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000
ota_data_initial.bin 0x20000 pedalboard.bin` — verify the log reaches
`Bootstrap complete; no display on this board` with `USB host installed=yes`.
