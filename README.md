# PBD Module — Progressive Bitcrush Delay for the Daisy Patch

A Eurorack effect for the [Electro-Smith Daisy Patch](https://electro-smith.com/daisy/patch):
a stereo feedback delay whose **repeats decay progressively into lo-fi**.
Each echo loses a few bits of resolution per repeat, so a clean pluck
dissolves — repeat by repeat — into digital dust.

```
 clean in ──► [ delay ] ──► 16-bit echo ──► 14-bit echo ──► … ──► 2-bit dust
                  ▲_______________feedback_______________|
```

Hold the **FREEZE** gate and the loop keeps degrading after the input stops —
a chord slowly crumbling into grit, then holding there.

## Panel map

| Control | Function |
|---|---|
| CTRL 1 — TIME | Delay time, 5 ms … 2 s (exponential; CV summed with knob) |
| CTRL 2 — REGEN | Feedback, 0 … 0.98 |
| CTRL 3 — CRUSH | Bits lost per repeat, 0 … 4 (`0` = clean digital delay) |
| CTRL 4 — MIX | Dry/wet balance |
| GATE IN 1 | **FREEZE** (hold high): input muted, tail loops and keeps degrading to the bit floor |
| GATE IN 2 | **TAP TEMPO**: two taps set the delay time (TIME knob retakes over when moved) |
| Encoder click | Toggle ping-pong stereo |
| Encoder hold (> 0.6 s) | Clear delay buffers |

The OLED shows all four parameters with bar meters, a live **TAIL** readout of
the current echo bit depth, and status (`FROZEN`, `PING-PONG`, `CLEAN DELAY`).

## Repo layout

```
pbd-module/
├── src/
│   ├── pbd_dsp.h   # Platform-independent DSP core (no libDaisy dependency)
│   └── main.cpp    # Daisy Patch firmware: audio callback, controls, OLED UI
├── tests/
│   ├── test_pbd.cpp    # Host unit tests (impulse-response verification)
│   ├── render_demo.cpp # Renders demo_pbd.wav so you can hear the effect
│   └── wav_writer.h    # Minimal WAV writer for the demo
├── docs/
│   ├── DESIGN.md   # Concept, DSP math, control map, UI spec, future ideas
│   ├── BUILD.md    # Toolchain setup, build, flashing, troubleshooting
│   └── USER_MANUAL.md  # Player's guide: panel map, techniques, tips
├── demo_pbd.wav    # 8 s demo: plucks dissolving into 3-bit dust
├── Makefile        # Firmware build (libDaisy)
└── deps/           # libDaisy (git submodule — see docs/BUILD.md §2)
```

## Quick start

```bash
# 1. Host tests for the DSP core (no ARM toolchain needed)
g++ -std=c++17 -O2 -I src tests/test_pbd.cpp -o /tmp/test_pbd && /tmp/test_pbd

# 2. Render the demo
g++ -std=c++17 -O2 -I src -I tests tests/render_demo.cpp -o /tmp/render_demo \
  && /tmp/render_demo   # writes demo_pbd.wav

# 3. Build the firmware (needs arm-none-eabi-gcc — see docs/BUILD.md)
make

# 4. Flash (module in DFU mode: hold BOOT, tap RESET, release BOOT)
make program-dfu
```

Full instructions: [docs/BUILD.md](docs/BUILD.md).
Design deep-dive: [docs/DESIGN.md](docs/DESIGN.md).
Player's guide: [docs/USER_MANUAL.md](docs/USER_MANUAL.md).

## Status

- [x] DSP core designed, implemented, and unit-tested on host (12 checks)
- [x] Demo WAV rendered
- [x] Daisy Patch firmware written (audio, controls, OLED UI)
- [x] Firmware compiles and links for ARM Cortex-M7 (`build/pbd.bin`, 105 KB)
- [x] Tested on hardware (flashed 2026-10-02; extensive field test 2026-10-03 across samples, sound generators, vocals, beats — performs as expected)

## License

MIT — see [LICENSE](LICENSE). Built on
[libDaisy](https://github.com/electro-smith/libDaisy) (MIT).
