// test_pbd.cpp — host unit tests for the ProgressiveBitcrushDelay core.
//
// Verifies the defining behavior of the module:
//   1. Each echo is quantized to a PROGRESSIVELY lower bit depth.
//   2. With crush rate = 0 the delay is bit-transparent (clean digital delay).
//   3. Frozen tails degrade down to the bit floor and stay there.
//
// Build & run:
//   g++ -std=c++17 -O2 -I src tests/test_pbd.cpp -o /tmp/test_pbd && /tmp/test_pbd
#include "pbd_dsp.h"
#include <cmath>
#include <cstdio>

static int  s_failures = 0;
static int  s_checks   = 0;

#define CHECK(cond, ...)                                              \
    do {                                                              \
        s_checks++;                                                   \
        if(!(cond)) {                                                 \
            s_failures++;                                             \
            printf("FAIL line %d: ", __LINE__);                       \
            printf(__VA_ARGS__);                                      \
            printf("\n");                                             \
        }                                                             \
    } while(0)

static constexpr float kSr       = 48000.0f;
static constexpr size_t kBufLen  = (size_t)(kSr * 2.0f); // 2 s buffers
static float s_buf[kBufLen];
static float s_gen[kBufLen];

static float QuantizeRef(float x, float bits)
{
    return ProgressiveBitcrushDelay::Crush(x, bits);
}

// Peak absolute value in a window around `center`.
static float PeakNear(const float* sig, size_t n, size_t center, size_t halfwin)
{
    float peak = 0.0f;
    size_t lo  = center > halfwin ? center - halfwin : 0;
    size_t hi  = center + halfwin < n ? center + halfwin : n - 1;
    for(size_t i = lo; i <= hi; i++)
    {
        float a = fabsf(sig[i]);
        if(a > peak)
            peak = a;
    }
    return peak;
}

static void TestProgressiveCrush()
{
    printf("-- TestProgressiveCrush\n");
    ProgressiveBitcrushDelay d;
    d.Init(kSr, s_buf, s_gen, kBufLen);
    d.SetDelaySeconds(0.1f);   // 4800 samples
    d.SetFeedback(0.9f);
    d.SetCrushRate(2.0f);      // lose 2 bits per repeat
    d.SetStartBits(16.0f);
    d.SetFloorBits(2.0f);
    d.SetDecimation(0.0f);

    // Warm up: let the smoothed delay time converge to its target so echoes
    // land exactly on multiples of the delay period.
    for(size_t i = 0; i < 30000; i++)
        d.Process(0.0f);

    static float out[kBufLen];
    const size_t n = kBufLen;
    for(size_t i = 0; i < n; i++)
    {
        float in = (i == 0) ? 0.8f : 0.0f; // impulse
        out[i]   = d.Process(in);
    }

    // Echo k should sit at bit depth max(2, 16 - 2k), amplitude ~= 0.8 * 0.9^k
    // quantized to that depth. Allow one quantization step of slop.
    for(int k = 0; k < 6; k++)
    {
        float bits     = 16.0f - 2.0f * k;
        if(bits < 2.0f)
            bits = 2.0f;
        float ideal    = 0.8f * powf(0.9f, k);
        float expected = QuantizeRef(ideal, bits);
        float step     = 2.0f / powf(2.0f, bits); // full-scale step
        float peak     = PeakNear(out, n, (size_t)((k + 1) * 4800), 240);
        CHECK(fabsf(peak - fabsf(expected)) <= step * 1.5f,
              "echo %d: peak %.5f, expected %.5f (bits %.0f, step %.5f)",
              k, peak, expected, bits, step);
    }
    // And the progression must be audible as coarsening: later echo steps bigger.
    float step0 = 2.0f / powf(2.0f, 16.0f - 2.0f * 0);
    float step5 = 2.0f / powf(2.0f, 16.0f - 2.0f * 5);
    CHECK(step5 > step0 * 100.0f, "quantization step grows along the tail");
}

static void TestCleanWhenCrushZero()
{
    printf("-- TestCleanWhenCrushZero\n");
    ProgressiveBitcrushDelay d;
    d.Init(kSr, s_buf, s_gen, kBufLen);
    d.SetDelaySeconds(0.05f); // 2400 samples
    d.SetFeedback(0.5f);
    d.SetCrushRate(0.0f); // no progression -> transparent delay
    d.SetDecimation(0.0f);

    for(size_t i = 0; i < 30000; i++)
        d.Process(0.0f); // let smoothed delay time settle

    static float out[kBufLen];
    const size_t n = (size_t)(kSr * 1.0f);
    for(size_t i = 0; i < n; i++)
    {
        float in = (i == 0) ? 0.7f : 0.0f;
        out[i]   = d.Process(in);
    }
    for(int k = 0; k < 4; k++)
    {
        float expected = 0.7f * powf(0.5f, k);
        float peak     = PeakNear(out, n, (size_t)((k + 1) * 2400), 120);
        CHECK(fabsf(peak - expected) < 1e-4f,
              "echo %d: peak %.6f, expected %.6f", k, peak, expected);
    }
}

static void TestFreezeDissolvesToFloor()
{
    printf("-- TestFreezeDissolvesToFloor\n");
    ProgressiveBitcrushDelay d;
    d.Init(kSr, s_buf, s_gen, kBufLen);
    d.SetDelaySeconds(0.02f); // short loop -> many generations, fast test
    d.SetFeedback(0.99f);
    d.SetCrushRate(3.0f);
    d.SetStartBits(16.0f);
    d.SetFloorBits(4.0f);
    d.SetDecimation(0.0f);

    // Charge the loop, then freeze and let it dissolve.
    for(size_t i = 0; i < (size_t)(kSr * 0.5f); i++)
        d.Process(i < 100 ? 0.6f : 0.0f);
    d.SetFrozen(true);
    for(size_t i = 0; i < (size_t)(kSr * 2.0f); i++)
        d.Process(0.0f);

    CHECK(fabsf(d.GetTailBits() - 4.0f) < 1e-3f,
          "frozen tail should rest at the 4-bit floor, got %.2f",
          d.GetTailBits());
}

int main()
{
    TestProgressiveCrush();
    TestCleanWhenCrushZero();
    TestFreezeDissolvesToFloor();
    printf("%d checks, %d failures\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
