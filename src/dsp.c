#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <zmq.h>
#include <math.h>

#include "dsp.h"

void int16_sine_wave(int32_t fs, int32_t fc, int16_t *buf, size_t buf_len)
{
    double w = 2 * PI * fc;
    for (int i = 0; i < buf_len; i++)
    {
        // Only 12 bits of data so values need to be capped at 4096 / 2
        double t = (double)i / fs;
        buf[i] = (int16_t)(sin(w * t) * 1800);
    }
}

void int16_zeros(int16_t *buf, size_t buf_len)
{
    for (int i = 0; i < buf_len; i++)
    {
        buf[i] = 0;
    }
}