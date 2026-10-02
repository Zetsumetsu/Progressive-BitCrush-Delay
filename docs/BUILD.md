# Build & Flash Guide

## 1. Install the toolchain

You need the ARM cross-compiler. (Git and Make are assumed.)

**macOS**
```bash
brew install armmbed/formulae/arm-none-eabi-gcc
```

**Linux (Debian/Ubuntu)**
```bash
sudo apt install gcc-arm-none-eabi make git
```
Note: libDaisy officially wants ARM GCC **10.3-2021.10**; distro packages
are usually fine for this firmware, but if you hit odd build errors, install
10.3-2021.10 from
[developer.arm.com](https://developer.arm.com/downloads/-/gnu-rm/10-3-2021-10)
and put its `bin/` on your `PATH`.

**Windows**
Use the [Daisy Toolchain installer](https://docs.daisy.audio/tutorials/toolchain-windows/)
([direct download, v1.1.0](https://daisy.nyc3.cdn.digitaloceanspaces.com/installers/DaisyToolchain-1.1.0-win64.exe)),
or install manually under MSYS2/MINGW64.

Verify:
```bash
arm-none-eabi-gcc --version
```

## 2. Get the source

```bash
git clone https://github.com/Zetsumetsu/Progressive-BitCrush-Delay.git pbd-module
cd pbd-module
git submodule add https://github.com/electro-smith/libDaisy.git deps/libDaisy
git submodule update --init --recursive
```
`libDaisy` (the only dependency) is added as a git submodule in `deps/`.

## 3. Build

```bash
make
```
Output: `build/pbd.elf`, `build/pbd.bin`.

The Makefile sets `APP_TYPE = BOOT_SRAM`: the app runs from SRAM via the
Daisy bootloader, which gives the firmware room to grow. (The DSP delay
memory itself lives in the Seed's 64 MB SDRAM, initialized by `patch.Init()`.)

### Sanity checks without hardware

The DSP core has no hardware dependency — test it on your host machine:

```bash
g++ -std=c++17 -O2 -I src tests/test_pbd.cpp -o /tmp/test_pbd && /tmp/test_pbd
# → "12 checks, 0 failures"

g++ -std=c++17 -O2 -I src -I tests tests/render_demo.cpp -o /tmp/render_demo \
  && /tmp/render_demo
# → writes demo_pbd.wav (8 s): plucks dissolving into 3-bit dust
```

## 4. Flash

### First time: install the Daisy bootloader (once per module)

`APP_TYPE = BOOT_SRAM` needs the Daisy bootloader in the Seed's internal
flash. Put the module in DFU mode (**hold BOOT, tap RESET, release BOOT**),
then from `deps/libDaisy/core`:

```bash
make program-boot
```

This replaces internal flash, so any previous `BOOT_NONE` firmware is
replaced. Afterwards the module always boots the bootloader, which waits
~2 s for a DFU upload before starting the app.

### Flash the firmware

**Over USB (needs `dfu-util`):**
```bash
# module in DFU mode (hold BOOT, tap RESET, release BOOT)
make program-dfu
```

**Via SD card (no dfu-util needed):**
Copy `build/pbd.bin` to the root of a **FAT32** SD card, insert it, and
power-cycle the module. The bootloader flashes the first `.bin` it finds
in the card root (only when it differs from what's loaded).

## 5. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| `arm-none-eabi-gcc: command not found` | Toolchain not installed or not on `PATH` (§1) |
| Build errors mentioning GCC version | Install ARM GCC 10.3-2021.10 specifically |
| `make program-dfu` can't find the device | Module not in DFU mode; try another USB cable (must be data-capable) |
| Module boots but no audio | Check Eurorack power ribbon orientation; confirm input signal present |
| SD card not flashing | Card must be FAT32 (not exFAT); `.bin` must be in the card **root** |
| Echoes sound clean with CRUSH up | CRUSH at 0 = clean by design; turn CTRL 3 clockwise |
| Delay time feels "steppy" | That's the TAP TEMPO takeover guard — move the TIME knob deliberately |

## 6. Iterating

Tweak `src/pbd_dsp.h` for DSP changes (test on host first — it's fast),
`tweak src/main.cpp` for panel/UI changes, then `make && make program-dfu`.
Keep the design doc (`docs/DESIGN.md`) in sync with behavior changes; future
you will thank present you.
