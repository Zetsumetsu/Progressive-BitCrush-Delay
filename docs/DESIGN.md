# PBD Design Document — Progressive Bitcrush Delay

*Status: v1 design, DSP core implemented & tested. Firmware written, awaiting
hardware test.*

## 1. The concept

A normal feedback delay repeats the input at a fixed fidelity until the
feedback decays below audibility. A bitcrusher reduces fidelity uniformly.
The **Progressive Bitcrush Delay** combines them so that *fidelity itself
decays*: the first repeat is (nearly) full resolution, and every subsequent
repeat is quantized more coarsely than the last, until the tail settles into
a bed of digital grit at a fixed bit floor.

The key insight: **track how many times each sample has circulated** — its
*generation* — and make the quantization depth a function of generation:

```
bits(g) = clamp(start_bits − g · crush_rate, floor_bits, 16)
```

With `crush_rate = 0` the module is a plain clean digital delay; turn it up
and the tail audibly crumbles. Because the depth is per-sample rather than
global, a newly played note always enters the loop at full fidelity even
while an older tail is still dissolving beneath it.

### Why not just put a bitcrusher in the feedback loop?

A fixed-depth crusher in the loop converges to its set depth within one or
two repeats — you hear a *step* into lo-fi, not a *progression*. True
progression needs the depth to deepen per repeat, which needs per-sample
generation tracking. That's the whole module, really.

## 2. DSP design (`src/pbd_dsp.h`)

Platform-independent C++ (no libDaisy dependency) so the core can be
unit-tested on a host machine. See `tests/test_pbd.cpp`.

### 2.1 Signal flow (per sample, per channel)

```
read  = delay[w − delay_samples]            fractional read, linear interp
gen   = generation[read_pos]                0 = fresh input
bits  = clamp(start − gen·rate, floor, 16)   progressive depth
held  = decimate(read, gen)                  optional progressive SR reduction
wet   = crush(held, bits)                    fractional-bit quantization
write = frozen ? wet·0.9995 : in + wet·feedback (+ crossfeed)
gen[w] = weighted blend of 0 and gen+1       (see 2.2)
```

### 2.2 Generation tracking — amplitude-weighted

A naive `gen[w] = gen[read] + 1` breaks musically: *silence* circulating in
the loop also accumulates generations, so a note played ten seconds after
power-up would enter the loop pre-crushed.

Instead the written generation is an amplitude-weighted blend:

```
gen_write = (|fb|·(gen_read + 1)) / (|in| + |fb|)      (|in|+|fb| > ε)
gen_write = gen_read + 1                               (silence)
```

Fresh input dominates → generation ≈ 0 → first echo at full `start_bits`.
Circulating tail alone → generation increments → progressive decay. A note
played *over* a decaying tail lands somewhere sensible in between.

### 2.3 Fractional-bit quantization

`crush_rate` is continuous (knob), so bit depth is fractional. `Crush(x, bits)`
quantizes at `floor(bits)` and `ceil(bits)` and interpolates — no zipper steps
as the CRUSH knob moves.

```cpp
levels = 2^(bits−1);  q = round(x·levels) / levels;   // x in [−1, 1]
```

### 2.4 Progressive sample-rate reduction

Optional companion to the bit-depth progression: a zero-order hold whose
length grows with generation (`hold = 1 + gen·decim·8`, capped at 256).
Fixed at a subtle 0.15 in firmware v1 — the smear rides along with the crush.

### 2.5 Delay-time smoothing

Delay time is smoothed per-sample toward its target (coefficient 0.001) with
a snap when within 0.5 samples. The snap matters: naive float smoothing
stalls ~0.5 samples from the target (float epsilon at these magnitudes),
which would leave *every* read permanently fractionally interpolated — a
constant unintended lowpass. With the snap, steady-state reads are exact.

### 2.6 Freeze behavior

Gate IN 1 high → input muted, loop recirculates at ~unity (`wet·0.9995`).
Generation keeps incrementing, so the frozen tail **dissolves into the bit
floor and holds there** — the module's signature moment. The 0.9995 factor
keeps the loop from running away while staying perceptually infinite.

## 3. Control map (Daisy Patch)

Four knobs, each hardware-summed with its CV input on the Patch:

| # | Name | Range | Mapping |
|---|------|-------|---------|
| 1 | TIME | 5 ms … 2 s | exponential: `0.005 · 400^k` |
| 2 | REGEN | 0 … 0.98 | linear |
| 3 | CRUSH | 0 … 4 bits/repeat | linear (`0` = clean delay) |
| 4 | MIX | 0 … 1 | linear dry/wet |

| Input | Behavior |
|-------|----------|
| GATE IN 1 | FREEZE while high |
| GATE IN 2 | TAP TEMPO — two taps (80 ms … 2.5 s apart) set delay time; TIME knob retakes over when moved > 4% (takeover guard) |
| Encoder click | Toggle ping-pong stereo (cross-feedback at 0.9·regen) |
| Encoder hold > 0.6 s | Clear delay buffers |

Fixed in v1: `start_bits = 16`, `floor_bits = 2`, `decimation = 0.15`,
max delay 2 s/channel (SDRAM).

## 4. OLED UI (128×64)

```
PBD // PROG BITCRUSH DLY
────────────────────────────────
TIME  0.42s            [████░░░░]
REGN  0.78             [██████░░]
CRSH  1.5b/r           [███░░░░░]
MIX   0.50  TAIL 11.2b
* FROZEN - dissolving *
```

- Four parameter rows with live bar meters.
- **TAIL** readout: bit depth of the most recently read echo — watch it count
  down as the tail degrades.
- Status line: `FROZEN` (dissolving), `PING-PONG`, or `CLEAN DELAY`
  (shown when CRUSH ≈ 0).
- UI refreshes at ~15 Hz from the main loop; audio untouched.

## 5. Verification

Host unit tests (`tests/test_pbd.cpp`, 12 checks, all passing):

1. **Progressive crush** — impulse in; echo *k* peaks at `0.8·0.9ᵏ`
   quantized to `16−2k` bits within 1.5 quantization steps, for k = 0…5.
2. **Clean mode** — `crush_rate = 0` gives bit-transparent echoes
   (within 1e−4).
3. **Freeze dissolve** — frozen tail's reported bit depth reaches the
   4-bit floor and rests there.

Plus `tests/render_demo.cpp`, which renders `demo_pbd.wav` (8 s stereo):
three plucked notes dissolving into 3-bit dust — listen before you flash.

## 6. Future ideas (not in v1)

- **MIDI clock sync** — subdivide delay time to incoming MIDI clock.
- **CRUSH CV curve shapes** — linear vs. exponential deepening per repeat.
- **Dotted/shuffle tap** — tap-tempo subdivisions on encoder turn.
- **Reverse tail** — read the loop backwards while frozen.
- **Stereo spread of generations** — L/R degrade at slightly different
  rates for a widening lo-fi image.
- **Preset slots** — encoder long-press menu, save to SD card.
- **Second page on OLED** — bit-depth histogram of the loop contents.
