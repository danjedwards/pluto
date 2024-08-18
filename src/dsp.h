#ifndef DSP
#define DSP

#include <stdio.h>
#include <unistd.h>
#include <math.h>

#define PI 3.141

void int16_sine_wave(int32_t fs, int32_t fc, int16_t *buf, int16_t len);

#endif