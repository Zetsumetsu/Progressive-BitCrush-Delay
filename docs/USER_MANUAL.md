# PBD User Manual — Progressive Bitcrush Delay

*For the Electro-Smith Daisy Patch. Firmware v1.*

## What it is

Most delays repeat your sound until it fades. Most bitcrushers smash
everything to one fixed lo-fi depth. The **Progressive Bitcrush Delay**
does something neither can: every repeat comes back a little more
degraded than the last. A clean pluck returns at near full resolution,
then 14 bits, then 12… until the tail settles into a bed of 2-bit
digital dust.

The trick is that the module tracks how many times each sample has
circulated the loop — its *generation* — and makes the quantization
depth a function of generation. New notes always enter at full fidelity,
even while an older tail is still dissolving underneath them.

With the CRUSH knob at zero, it's a plain clean digital delay. Turn it
up and the tail audibly crumbles.

---

## Panel map

| Control | Function |
|---|---|
| **CTRL 1 — TIME** | Delay time, 5 ms … 2 s (exponential). CV input sums with the knob. |
| **CTRL 2 — REGEN** | Feedback amount, 0 … 0.98. Higher = longer tails = deeper dissolution. |
| **CTRL 3 — CRUSH** | Bits lost per repeat, 0 … 4. `0` = clean delay. This is the soul of the module. |
| **CTRL 4 — MIX** | Dry/wet balance. |
| **GATE IN 1** | **FREEZE** — hold high: input mutes, the loop recirculates and keeps degrading down to the bit floor, then holds there. |
| **GATE IN 2** | **TAP TEMPO** — two taps set the delay time. Moving the TIME knob more than ~4% hands control back to the knob. |
| **Encoder click** | Toggle **ping-pong** stereo (cross-feedback between channels). |
| **Encoder hold (> 0.6 s)** | **Clear** the delay buffers. |

All four knobs are summed with their CV inputs, so everything is
playable by hand *and* by modulation.

## The OLED

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
- **TAIL** readout: the bit depth of the most recently read echo. Watch
  it count down as the tail degrades — it's the module thinking out loud.
- Status line: `FROZEN` (dissolving), `PING-PONG`, or `CLEAN DELAY`
  (when CRUSH is near zero).

---

## Getting started

1. Start with CRUSH at 0 and dial in a normal delay: TIME to taste,
   REGEN around 0.5–0.7, MIX to taste.
2. Slowly raise CRUSH. Around 1–1.5 bits/repeat you'll hear the tail
   start to fray at the edges. Past 2.5 it crumbles fast.
3. Feed it something with a sharp attack — a pluck, a rimshot, a
   chopped vocal — and listen to each repeat step down in resolution.
4. Hold **FREEZE** (GATE IN 1 high) and let go of the source. The
   captured loop keeps circulating and keeps degrading until it rests
   in the bit floor. Release the gate to let new audio back in.

**First patch to try:** vocal sample → PBD → out. TIME ~0.4 s,
REGEN ~0.75, CRUSH ~1.5, MIX ~0.6. Play a phrase, then hit FREEZE on
the last word and ride the CRUSH knob by hand as it dissolves.

---

## Playing techniques

### Hand-riding the dissolve (the signature move)

The CRUSH knob is continuous — no stepped zippering as you turn it —
so it plays like an instrument. Capture a vocal phrase with FREEZE,
then slowly open CRUSH by hand and feel the words come apart in your
fingers. Small movements near 1 bit/repeat give you long, mournful
decays; pushing toward 3–4 collapses the tail into grit within a few
repeats. Pulling CRUSH back mid-dissolve won't restore lost fidelity
(the generations are already spent), but it slows the ongoing decay —
like easing off the accelerator downhill.

### LFO → CV: animated degradation

Patch an LFO into the CRUSH CV input and the *rate of decay itself*
breathes. Slow LFOs (0.05–0.2 Hz) make the tail alternately smear and
sharpen over many repeats — the loop seems to remember and forget.
Audio-rate or fast LFOs into TIME CV give classic warbly pitch-drifting
repeats, now with the added dimension of progressive lo-fi.

### Thresholding LFO amplitudes: rhythmic glitch beats

This is where it gets weird in the best way. Instead of letting an LFO
sweep smoothly, gate or threshold its amplitude — only letting it
through above a set level — and use that chopped modulation to drive
CRUSH or TIME CV. The result: the degradation arrives in rhythmic
bursts, and the tail stutters between clean and crushed in patterns
that feel composed rather than random.

- **Single LFO, thresholded:** regular, hypnotic gating of the crush —
  the tail pulses between hi-fi and dust on the LFO's period.
- **Two LFOs at different rates, both thresholded:** the gates interleave
  into off-kilter polyrhythms. The glitch pattern drifts in and out of
  phase with your beat — endlessly non-repeating texture.

Try it on a drum loop: TIME synced roughly to the beat (tap it in),
REGEN ~0.8, CRUSH base ~1, with a thresholded LFO pushing CRUSH CV.
Every few bars the beat seems to disintegrate and reassemble.

### Ping-pong + freeze: the widening dissolve

Click the encoder for ping-pong, then freeze a wide stereo source.
The echoes bounce left-right while each bounce loses bits — the stereo
image stays alive even as the fidelity dies. Gorgeous on pads and
reverb tails.

---

## Know your source material

The PBD doesn't treat all audio equally — the effect is a conversation
between the algorithm and the source. Field testing found:

- **Vocal samples:** the standout. Formants survive deep into the
  crush, so words stay ghostly intelligible long after they'd normally
  be gone. The most rewarding material for hand-riding the dissolve.
- **Beats and sharp transients:** excellent for the glitch techniques
  above. Transients give the degradation something to bite into; each
  hit re-triggers the ear before dissolving.
- **Dense, sustained material** (thick pads, full mixes): tends to
  smear into uniform texture rather than articulate repeats. Still
  beautiful as a wash, but subtler — lower CRUSH and longer TIME work
  best here.
- **Sparse plucks and mallet sounds:** the textbook demo — each repeat
  audibly steps down, and you can count the generations by ear.

If a patch sounds muddy rather than magical, the source is usually the
reason, not the settings. Feed it something with space and attack
first, then work toward denser material.

---

## Tips

- **TAIL is your compass.** If the readout parks at 2.0b and never
  moves, your REGEN is too low for the tail to circulate, or CRUSH is
  at zero. If it never gets *above* ~6b, back CRUSH off — you're
  crushing the first repeat too hard to enjoy the progression.
- **Tap tempo + knob takeover:** after tapping, the TIME knob is
  dormant until you move it deliberately (~4% change), so you won't
  accidentally nudge it mid-performance.
- **Stuck in the mud?** Encoder hold (> 0.6 s) clears the buffers
  instantly — the panic button for runaway frozen loops.
- **Stereo image:** the two channels degrade independently per their
  own signal, so stereo sources develop slight L/R differences in the
  tail — a natural widening, not a bug.
- **First boot:** if the OLED ever glitches on power-up, a quick power
  cycle clears it (display init quirk, harmless).

---

## Specs (v1 firmware)

- Delay time: 5 ms … 2 s per channel (exponential), fractional-read
  with linear interpolation
- Feedback: 0 … 0.98, per-channel (0.9× crossfeed in ping-pong)
- Crush: 0 … 4 bits lost per repeat, continuous (fractional-bit
  quantization — no stepping)
- Bit depth range: 16 bits (fresh) → 2-bit floor, per-sample
  generation-tracked
- Extras: progressive sample-rate reduction rides along subtly with
  the crush; freeze recirculates at ~unity for a perceptually
  infinite (but ever-degrading) hold
- Max delay memory: 2 s/channel

*PBD — Progressive Bitcrush Delay. Play the decay.*
