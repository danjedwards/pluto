#ifndef PLUTO
#define PLUTO

#include <iio.h>

#define MHZ(x) ((long long)(x * 1000000.0 + .5))
#define GHZ(x) ((long long)(x * 1000000000.0 + .5))

#define BUFFER_SIZE 1024 * 256

enum iodev
{
    RX,
    TX
};

struct stream_cfg
{
    long long bw_hz;    // Analog banwidth in Hz
    long long fs_hz;    // Baseband sample rate in Hz
    long long lo_hz;    // Local oscillator frequency in Hz
    const char *rfport; // Port name
};

struct pluto_dev
{
    struct iio_context *ctx;
    struct iio_channel *rx0_i;
    struct iio_channel *rx0_q;
    struct iio_channel *tx0_i;
    struct iio_channel *tx0_q;
    struct iio_buffer *rxbuf;
    struct iio_buffer *txbuf;
    struct iio_device *tx;
    struct iio_device *rx;
};

struct pluto_dev pluto_init(const char *ctx_uri, struct stream_cfg txcfg, struct stream_cfg rxcfg);

size_t pluto_get_tx_buf_len(struct pluto_dev *pluto);
void pluto_tx(struct pluto_dev *pluto, int16_t *buf);

size_t pluto_get_rx_buf_len(struct pluto_dev *pluto);
void pluto_rx(struct pluto_dev *pluto, int16_t *buf);

void pluto_shutdown(struct pluto_dev *pluto);

#endif