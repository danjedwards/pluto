#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <zmq.h>
#include <math.h>

#include "dsp.h"

void int16_sine_wave(int32_t fs, int32_t fc, int16_t *buf, int16_t len)
{
    double w = 2 * PI * fc;
    for (int i = 0; i < len; i++)
    {
        double t = (double)i / fs;
        buf[i] = (int16_t)(sin(w * t) * 32767);
    }
}

#ifdef DSP_TARGET // Used to test dsp functions. Only compiled if dsp_test is the build target.
int main()
{

    void *zmq_ctx = zmq_ctx_new();
    void *publisher = zmq_socket(zmq_ctx, ZMQ_PUB);

    int rc = zmq_bind(publisher, "tcp://127.0.0.1:5556");
    assert(rc == 0);

    int32_t fs = 30719999;
    int32_t fc = 500000;
    int16_t buf_len = 4086;
    int16_t buf[buf_len];
    int16_sine_wave(fs, fc, buf, buf_len);

    while (1)
    {
        zmq_send(publisher, buf, buf_len, 0);
        sleep(1);
    }

    return 0;
}
#endif