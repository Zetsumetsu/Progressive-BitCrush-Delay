// Minimal 16-bit PCM WAV writer (mono or stereo, float -1..1 in).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

inline bool WriteWav(const char*  path,
                     const float* const* ch,
                     int          num_ch,
                     int          num_frames,
                     int          sample_rate)
{
    FILE* f = fopen(path, "wb");
    if(!f)
        return false;
    const int      bytes_per_sample = 2;
    const uint32_t data_bytes = (uint32_t)num_frames * num_ch * bytes_per_sample;
    const uint32_t riff_size  = 36 + data_bytes;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_fmt = 1, channels = (uint16_t)num_ch;
    uint32_t sr = (uint32_t)sample_rate;
    uint32_t byte_rate = sr * num_ch * bytes_per_sample;
    uint16_t block_align = (uint16_t)(num_ch * bytes_per_sample);
    uint16_t bps = 16;
    fwrite(&audio_fmt, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);
    for(int i = 0; i < num_frames; i++)
        for(int c = 0; c < num_ch; c++)
        {
            float v = ch[c][i];
            v = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
            int16_t s = (int16_t)(v * 32767.0f);
            fwrite(&s, 2, 1, f);
        }
    fclose(f);
    return true;
}
