/*
 * pbd_dsp.h — Progressive Bitcrush Delay (PBD) DSP core
 *
 * A feedback delay in which every repeat is a little more destroyed than
 * the last. Each sample circulating in the delay line carries a "generation"
 * counter: how many times it has passed through the loop. The bit depth used
 * to quantize the echo is a function of that generation, so the tail of the
 * delay audibly crumbles from hi-fi into digital dust — progressively, one
 * repeat at a time.
 *
 * Platform-independent: no libDaisy dependency, so this file can be compiled
 * and unit-tested on a host machine (see tests/).
 *
 * Signal flow (per sample):
 *
 *   read  = delay[(w - delay_samples)]          (fractional, linear interp)
 *   gen   = generation of that read position:
 *           0 for freshly-written input, +1 per feedback circulation.
 *           Fresh input resets toward 0 via amplitude weighting, so a new
 *           note always enters the loop at full fidelity no matter how long
 *           the module has been running.
 *   bits  = clamp(start_bits - gen * crush_rate, floor_bits, 16)
 *   wet   = crush(decimate(read), bits)         (fractional-bit interpolation)
 *   write = frozen ? wet * 0.9995
 *                  : in + wet * feedback + fb_insert
 *   gen[w] = amplitude-weighted blend of 0 (fresh input) and gen + 1 (tail)
 *
 * Memory: caller provides two buffers of max_samples floats (audio + gen).
 * On the Daisy Seed, place these in SDRAM with DSY_SDRAM_BSS.
 */
#pragma once
#ifndef PBD_DSP_H
#define PBD_DSP_H

#include <cmath>
#include <cstdint>

class ProgressiveBitcrushDelay
{
  public:
    ProgressiveBitcrushDelay() {}
    ~ProgressiveBitcrushDelay() {}

    /** Init with external buffers. max_samples must be >= delay range needed. */
    void Init(float sample_rate, float* delay_buf, float* gen_buf, size_t max_samples)
    {
        sample_rate_ = sample_rate;
        buf_         = delay_buf;
        gen_         = gen_buf;
        max_len_     = max_samples;
        Reset();
        SetDelaySeconds(0.4f);
        SetFeedback(0.7f);
        SetCrushRate(1.0f);
        SetStartBits(16.0f);
        SetFloorBits(3.0f);
        SetDecimation(0.0f);
        SetFrozen(false);
    }

    void Reset()
    {
        for(size_t i = 0; i < max_len_; i++)
        {
            buf_[i] = 0.0f;
            gen_[i] = 0.0f;
        }
        write_idx_    = 0;
        delay_current_ = delay_target_ = sample_rate_ * 0.4f;
        hold_value_    = 0.0f;
        hold_count_    = 0;
        tail_bits_     = 16.0f;
    }

    /** Delay time in seconds. Clamped to [5ms, buffer length]. */
    void SetDelaySeconds(float seconds)
    {
        float max_s = (float)max_len_ / sample_rate_;
        if(seconds < 0.005f)
            seconds = 0.005f;
        if(seconds > max_s)
            seconds = max_s;
        delay_target_ = seconds * sample_rate_;
    }

    /** Feedback amount, 0..1 (clamped to 0.999 for stability). */
    void SetFeedback(float fb)
    {
        feedback_ = fb < 0.0f ? 0.0f : (fb > 0.999f ? 0.999f : fb);
    }

    /** Bits of resolution lost per repeat. 0 = clean digital delay. */
    void SetCrushRate(float bits_per_repeat)
    {
        crush_rate_ = bits_per_repeat < 0.0f ? 0.0f
                      : bits_per_repeat > 8.0f ? 8.0f
                                              : bits_per_repeat;
    }

    /** Bit depth of the first repeat (the "hi-fi" end). */
    void SetStartBits(float bits)
    {
        start_bits_ = bits < 4.0f ? 4.0f : (bits > 16.0f ? 16.0f : bits);
    }

    /** Bit depth floor — the dust the tail settles into. Never goes lower. */
    void SetFloorBits(float bits)
    {
        floor_bits_ = bits < 1.0f ? 1.0f : (bits > 8.0f ? 8.0f : bits);
    }

    /**
     * Progressive sample-rate reduction, 0..1.
     * Hold length (in samples) grows with generation: hold = 1 + gen * d * 8.
     * 0 disables it (pure bit-depth progression).
     */
    void SetDecimation(float d)
    {
        decim_ = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
    }

    void SetFrozen(bool frozen) { frozen_ = frozen; }

    /**
     * Process one sample. Returns the wet (delayed, crushed) signal.
     * fb_insert is an extra signal injected into the feedback loop —
     * used for ping-pong cross-feedback. Dry/wet mixing is left to the caller.
     */
    float Process(float in, float fb_insert = 0.0f)
    {
        // Smooth delay-time changes to avoid zipper clicks; snap when close
        // (float precision would otherwise stall the glide ~0.5 samples out,
        // leaving every read permanently interpolated).
        float dDelta = delay_target_ - delay_current_;
        if(fabsf(dDelta) < 0.5f)
            delay_current_ = delay_target_;
        else
            delay_current_ += 0.001f * dDelta;

        // Fractional read position with linear interpolation.
        float rpos = (float)write_idx_ - delay_current_;
        while(rpos < 0.0f)
            rpos += (float)max_len_;
        size_t i0   = (size_t)rpos;
        size_t i1   = i0 + 1 < max_len_ ? i0 + 1 : 0;
        float  frac = rpos - (float)i0;
        float  read = buf_[i0] + frac * (buf_[i1] - buf_[i0]);
        float  gen  = gen_[i0];
        if(gen > 4096.0f)
            gen = 4096.0f;

        // Progressive bit depth: each generation loses crush_rate_ bits.
        // Generation 0 (fresh input) plays at full start_bits.
        float bits = start_bits_ - gen * crush_rate_;
        if(bits < floor_bits_)
            bits = floor_bits_;
        if(bits > 16.0f)
            bits = 16.0f;
        tail_bits_ = bits;

        // Progressive decimation: zero-order hold, hold time grows per repeat.
        float crushed;
        if(decim_ > 0.0f)
        {
            int target_hold = 1 + (int)(gen * decim_ * 8.0f);
            if(target_hold > 256)
                target_hold = 256;
            if(--hold_count_ <= 0)
            {
                hold_value_ = read;
                hold_count_ = target_hold;
            }
            crushed = Crush(hold_value_, bits);
        }
        else
        {
            crushed = Crush(read, bits);
        }

        // Write back: input + crushed feedback. The crushed value stored is
        // what the next generation reads, so degradation compounds naturally.
        float fb_sig    = frozen_ ? crushed * 0.9995f
                                  : crushed * feedback_ + fb_insert;
        float write_val = frozen_ ? fb_sig : in + fb_sig;
        if(write_val > 4.0f)
            write_val = 4.0f;
        else if(write_val < -4.0f)
            write_val = -4.0f;
        // Generation of the written sample: amplitude-weighted blend of
        // 0 (fresh input) and gen + 1 (circulating tail). A new note entering
        // the loop therefore starts at full fidelity; the tail keeps aging.
        // (Input is muted while frozen, so the tail dissolves on its own.)
        float in_amp   = frozen_ ? 0.0f : fabsf(in);
        float fb_amp   = fabsf(fb_sig);
        float denom    = in_amp + fb_amp;
        float gen_next = (denom > 1e-6f) ? (fb_amp * (gen + 1.0f)) / denom
                                        : gen + 1.0f;
        buf_[write_idx_] = write_val;
        gen_[write_idx_] = gen_next;
        if(++write_idx_ >= max_len_)
            write_idx_ = 0;

        return crushed;
    }

    /** Bit depth of the most recently read echo — useful for the display. */
    float GetTailBits() const { return tail_bits_; }

    /** Current (smoothed) delay time in seconds — useful for the display. */
    float GetDelaySeconds() const { return delay_current_ / sample_rate_; }

    /** Quantize x in [-1,1] to the given bit depth (fractional bits supported). */
    static float Crush(float x, float bits)
    {
        if(bits >= 15.99f)
            return x;
        if(x > 1.0f)
            x = 1.0f;
        else if(x < -1.0f)
            x = -1.0f;
        float lo = floorf(bits);
        float hi = ceilf(bits);
        if(hi < 1.0f)
            hi = 1.0f;
        if(lo < 1.0f)
            lo = 1.0f;
        float qlo = QuantizeBits(x, (int)lo);
        if(hi == lo)
            return qlo;
        float qhi = QuantizeBits(x, (int)hi);
        return qlo + (bits - lo) * (qhi - qlo);
    }

  private:
    static float QuantizeBits(float x, int bits)
    {
        // bits levels across [-1, 1]: step = 2 / 2^bits
        float levels = ldexpf(1.0f, bits - 1); // 2^(bits-1)
        float q      = roundf(x * levels) / levels;
        if(q > 1.0f)
            q = 1.0f;
        else if(q < -1.0f)
            q = -1.0f;
        return q;
    }

    float  sample_rate_   = 48000.0f;
    float* buf_           = nullptr;
    float* gen_           = nullptr;
    size_t max_len_       = 0;
    size_t write_idx_     = 0;
    float  delay_target_  = 0.0f;
    float  delay_current_ = 0.0f;
    float  feedback_      = 0.7f;
    float  crush_rate_    = 1.0f;
    float  start_bits_    = 16.0f;
    float  floor_bits_    = 3.0f;
    float  decim_         = 0.0f;
    bool   frozen_        = false;
    float  hold_value_    = 0.0f;
    int    hold_count_    = 0;
    float  tail_bits_     = 16.0f;
};

#endif // PBD_DSP_H
