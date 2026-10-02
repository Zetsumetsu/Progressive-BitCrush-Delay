// render_demo.cpp — render a demo WAV of the Progressive Bitcrush Delay.
//
// A few plucked notes go in; a tail that crumbles into digital dust comes out.
// Build & run:
//   g++ -std=c++17 -O2 -I src -I tests tests/render_demo.cpp -o /tmp/render_demo \
//       && /tmp/render_demo && ls -la demo_pbd.wav
#include "pbd_dsp.h"
#include "wav_writer.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

static constexpr float kSr = 48000.0f;

int main()
{
    static float bufL[96000], genL[96000], bufR[96000], genR[96000];
    ProgressiveBitcrushDelay dlyL, dlyR;
    dlyL.Init(kSr, bufL, genL, 96000);
    dlyR.Init(kSr, bufR, genR, 96000);

    // A musical setting: dotted-eighth-ish delay, strong regen, audible crumble.
    for(auto* d : {&dlyL, &dlyR})
    {
        d->SetDelaySeconds(0.42f);
        d->SetFeedback(0.82f);
        d->SetCrushRate(1.2f);   // ~1.2 bits lost per repeat
        d->SetStartBits(16.0f);
        d->SetFloorBits(3.0f);   // tails settle into 3-bit grit
        d->SetDecimation(0.15f); // gentle progressive sample-rate smear
    }

    const int   seconds = 8;
    const int   frames  = (int)(kSr * seconds);
    static float outL[48000 * 8], outR[48000 * 8];

    // Input: three plucked notes (decaying harmonic bursts), A2 / C#3 / E3.
    const float notes[3]    = {110.0f, 138.59f, 164.81f};
    const int   onsets[3]   = {(int)(0.1f * kSr), (int)(1.1f * kSr), (int)(2.1f * kSr)};
    const float mix         = 0.45f;

    for(int i = 0; i < frames; i++)
    {
        float t  = (float)i / kSr;
        float in = 0.0f;
        for(int n = 0; n < 3; n++)
        {
            float dt = t - (float)onsets[n] / kSr;
            if(dt > 0.0f && dt < 1.2f)
            {
                float env = expf(-dt * 6.0f);
                in += env * 0.33f
                      * (sinf(2.0f * 3.14159265f * notes[n] * dt)
                         + 0.4f * sinf(2.0f * 3.14159265f * notes[n] * 2.0f * dt)
                         + 0.15f * sinf(2.0f * 3.14159265f * notes[n] * 3.0f * dt));
            }
        }
        float wetL = dlyL.Process(in);
        float wetR = dlyR.Process(in * 0.9f);
        outL[i]    = in * (1.0f - mix) + wetL * mix;
        outR[i]    = in * (1.0f - mix) + wetR * mix;
    }

    const float* ch[2] = {outL, outR};
    if(WriteWav("demo_pbd.wav", ch, 2, frames, (int)kSr))
        printf("wrote demo_pbd.wav (%d s stereo)\n", seconds);
    else
        printf("failed to write wav\n");
    return 0;
}
