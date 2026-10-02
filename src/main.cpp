/*
 * main.cpp — PBD Module firmware for the Electro-Smith Daisy Patch
 *
 * Progressive Bitcrush Delay: a stereo feedback delay whose repeats decay
 * progressively into lo-fi. Each echo loses a few bits of resolution per
 * repeat until it settles into digital dust at the bit-depth floor.
 *
 * PANEL MAP (Daisy Patch — knobs sum with their CV input in hardware)
 *
 *   CTRL 1 — TIME   delay time, 5 ms .. 2 s (exponential)
 *   CTRL 2 — REGEN  feedback, 0 .. 0.98
 *   CTRL 3 — CRUSH  bits lost per repeat, 0 .. 4  (0 = clean digital delay)
 *   CTRL 4 — MIX    dry/wet balance
 *
 *   GATE IN 1 — FREEZE (hold high): loops the tail, input muted, the frozen
 *               echo keeps degrading until it reaches the bit floor
 *   GATE IN 2 — TAP TEMPO: two taps set the delay time (knob retakes over
 *               when moved)
 *   ENCODER CLICK — toggle ping-pong stereo
 *   ENCODER HOLD (>0.6 s) — clear the delay buffers
 *
 * Build:  make
 * Flash:  make program-dfu   (module in DFU mode: hold BOOT, tap RESET)
 *         — or copy build/pbd.bin to a FAT32 SD card for the Daisy bootloader
 */
#include "daisy_patch.h"
#include "pbd_dsp.h"
#include <cstdio>
#include <cstring>

using namespace daisy;

DaisyPatch patch;

/* ------------------------------------------------------------------ */
/* Delay memory: 2 s per channel, stereo audio + stereo generation map */
/* ------------------------------------------------------------------ */
static constexpr float  kSampleRate   = 48000.0f;
static constexpr float  kMaxDelaySec  = 2.0f;
static constexpr size_t kMaxSamples   = (size_t)(kSampleRate * kMaxDelaySec);
static constexpr float  kDecimation   = 0.15f; // progressive SR reduction
static constexpr float  kFloorBits    = 2.0f;  // where the dust settles
static constexpr float  kStartBits    = 16.0f;

float DSY_SDRAM_BSS s_delayL[kMaxSamples];
float DSY_SDRAM_BSS s_delayR[kMaxSamples];
float DSY_SDRAM_BSS s_genL[kMaxSamples];
float DSY_SDRAM_BSS s_genR[kMaxSamples];

ProgressiveBitcrushDelay dlyL, dlyR;

/* ------------------------------------------------------------------ */
/* Shared state between the audio callback and the main loop            */
/* ------------------------------------------------------------------ */
struct Params
{
    float time_sec = 0.4f;
    float feedback = 0.7f;
    float crush    = 1.0f; // bits lost per repeat
    float mix      = 0.5f;
    bool  frozen   = false;
    bool  pingpong = false;
};
static Params     s_params;
static float      s_tapTimeSec   = -1.0f; // -1 = knob in charge
static float      s_knobAtTap    = 0.0f;
static uint32_t   s_lastTapMs    = 0;
static bool       s_holdClearArmed = true;

/* Exponential mapping for the TIME knob: 5 ms .. 2 s */
static float KnobToTime(float k)
{
    return 0.005f * powf(kMaxDelaySec / 0.005f, k);
}

static void ReadControls()
{
    float kTime  = patch.GetKnobValue(DaisyPatch::CTRL_1);
    float kRegen = patch.GetKnobValue(DaisyPatch::CTRL_2);
    float kCrush = patch.GetKnobValue(DaisyPatch::CTRL_3);
    float kMix   = patch.GetKnobValue(DaisyPatch::CTRL_4);

    // Tap tempo owns the delay time until the TIME knob is moved.
    if(s_tapTimeSec > 0.0f)
    {
        if(fabsf(kTime - s_knobAtTap) > 0.04f)
            s_tapTimeSec = -1.0f; // knob takes over again
        else
            s_params.time_sec = s_tapTimeSec;
    }
    if(s_tapTimeSec < 0.0f)
        s_params.time_sec = KnobToTime(kTime);

    s_params.feedback = kRegen * 0.98f;
    s_params.crush    = kCrush * 4.0f;
    s_params.mix      = kMix;
}

/* ------------------------------------------------------------------ */
/* Audio callback                                                       */
/* ------------------------------------------------------------------ */
static float s_wetLprev = 0.0f;
static float s_wetRprev = 0.0f;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    patch.ProcessAnalogControls();
    ReadControls();

    s_params.frozen = patch.gate_input[DaisyPatch::GATE_IN_1].State();

    dlyL.SetDelaySeconds(s_params.time_sec);
    dlyR.SetDelaySeconds(s_params.time_sec);
    dlyL.SetFeedback(s_params.feedback);
    dlyR.SetFeedback(s_params.feedback);
    dlyL.SetCrushRate(s_params.crush);
    dlyR.SetCrushRate(s_params.crush);
    dlyL.SetFrozen(s_params.frozen);
    dlyR.SetFrozen(s_params.frozen);

    const float xfb = s_params.pingpong ? s_params.feedback * 0.9f : 0.0f;
    const float mix = s_params.mix;

    for(size_t i = 0; i < size; i++)
    {
        float inL = in[0][i];
        float inR = in[1][i];

        float wetL = dlyL.Process(inL, xfb * s_wetRprev);
        float wetR = dlyR.Process(inR, xfb * s_wetLprev);
        s_wetLprev = wetL;
        s_wetRprev = wetR;

        out[0][i] = inL * (1.0f - mix) + wetL * mix;
        out[1][i] = inR * (1.0f - mix) + wetR * mix;
    }
}

/* ------------------------------------------------------------------ */
/* OLED UI                                                              */
/* ------------------------------------------------------------------ */
static void DrawBar(uint8_t x, uint8_t y, uint8_t w, float v)
{
    patch.display.DrawRect(x, y, x + w, y + 5, true);
    uint8_t fill = (uint8_t)(v * (float)w);
    for(uint8_t i = 0; i < fill; i++)
        patch.display.DrawLine(x + 1 + i, y + 1, x + 1 + i, y + 4, true);
}

static void UpdateDisplay()
{
    char line[32];
    patch.display.Fill(false);

    patch.display.SetCursor(0, 0);
    patch.display.WriteString("PBD // PROG BITCRUSH DLY", Font_7x10, true);
    patch.display.DrawLine(0, 11, 127, 11, true);

    // TIME
    snprintf(line, sizeof(line), "TIME %0.2fs", s_params.time_sec);
    patch.display.SetCursor(0, 15);
    patch.display.WriteString(line, Font_7x10, true);
    DrawBar(78, 15, 48, logf(s_params.time_sec / 0.005f) / logf(kMaxDelaySec / 0.005f));

    // REGEN
    snprintf(line, sizeof(line), "REGN %0.2f", s_params.feedback);
    patch.display.SetCursor(0, 26);
    patch.display.WriteString(line, Font_7x10, true);
    DrawBar(78, 26, 48, s_params.feedback / 0.98f);

    // CRUSH
    snprintf(line, sizeof(line), "CRSH %0.1fb/r", s_params.crush);
    patch.display.SetCursor(0, 37);
    patch.display.WriteString(line, Font_7x10, true);
    DrawBar(78, 37, 48, s_params.crush / 4.0f);

    // MIX + tail bit-depth meter
    snprintf(line, sizeof(line), "MIX %0.2f TAIL %0.1fb", s_params.mix, dlyL.GetTailBits());
    patch.display.SetCursor(0, 48);
    patch.display.WriteString(line, Font_7x10, true);

    // Status line
    patch.display.SetCursor(0, 57 - 2);
    if(s_params.frozen)
        patch.display.WriteString("* FROZEN - dissolving *", Font_7x10, true);
    else if(s_params.pingpong)
        patch.display.WriteString("PING-PONG", Font_7x10, true);
    else if(s_params.crush < 0.01f)
        patch.display.WriteString("CLEAN DELAY", Font_7x10, true);

    patch.display.Update();
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    patch.Init();
    patch.SetAudioBlockSize(48);

    dlyL.Init(kSampleRate, s_delayL, s_genL, kMaxSamples);
    dlyR.Init(kSampleRate, s_delayR, s_genR, kMaxSamples);
    dlyL.SetDecimation(kDecimation);
    dlyR.SetDecimation(kDecimation);
    dlyL.SetStartBits(kStartBits);
    dlyR.SetStartBits(kStartBits);
    dlyL.SetFloorBits(kFloorBits);
    dlyR.SetFloorBits(kFloorBits);

    patch.StartAdc();
    patch.StartAudio(AudioCallback);

    uint32_t lastUiMs = 0;
    for(;;)
    {
        patch.ProcessDigitalControls();

        // Encoder click: toggle ping-pong. Hold > 0.6 s: clear buffers.
        if(patch.encoder.RisingEdge())
        {
            s_params.pingpong = !s_params.pingpong;
            s_holdClearArmed  = true;
        }
        if(patch.encoder.Pressed() && patch.encoder.TimeHeldMs() > 600.0f
           && s_holdClearArmed)
        {
            dlyL.Reset();
            dlyR.Reset();
            s_holdClearArmed = false;
        }

        // Gate 2: tap tempo (80 ms .. 2.5 s between taps).
        if(patch.gate_input[DaisyPatch::GATE_IN_2].Trig())
        {
            uint32_t now = System::GetNow();
            uint32_t dt  = now - s_lastTapMs;
            if(s_lastTapMs != 0 && dt > 80 && dt < 2500)
            {
                s_tapTimeSec = (float)dt / 1000.0f;
                if(s_tapTimeSec > kMaxDelaySec)
                    s_tapTimeSec = kMaxDelaySec;
                s_knobAtTap = patch.GetKnobValue(DaisyPatch::CTRL_1);
            }
            s_lastTapMs = now;
        }

        // ~15 Hz UI refresh.
        uint32_t now = System::GetNow();
        if(now - lastUiMs > 66)
        {
            lastUiMs = now;
            UpdateDisplay();
        }
    }
}
