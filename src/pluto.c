#include <stdio.h>
#include <stdbool.h>
#include <iio.h>

#include "pluto.h"

/////////////////////////////////////////////////////////////////////////////////////
// iio
/////////////////////////////////////////////////////////////////////////////////////

#define IIO_ENSURE(expr)                                                             \
    {                                                                                \
        if (!(expr))                                                                 \
        {                                                                            \
            (void)fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
            (void)abort();                                                           \
        }                                                                            \
    }

static char tmpstr[64]; // static scratch mem for strings

static void errchk(int v, const char *what) // <------ FIX ME!!!
{
    if (v < 0)
    {
        fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what);
        // pluto_shutdown();

        printf("*******BAD! Could not shutdown properly because I am too lazy to think of a better solution atm.\n");
        exit(-1);
    }
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char *what, long long val)
{
    errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char *what, const char *str)
{
    errchk(iio_channel_attr_write(chn, what, str), what);
}

/* helper function generating channel names */
static char *get_ch_name(const char *type, int id)
{
    snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
    return tmpstr;
}

/* returns ad9361 phy device */
static struct iio_device *get_ad9361_phy(struct iio_context *ctx)
{
    struct iio_device *dev = iio_context_find_device(ctx, "ad9361-phy");
    IIO_ENSURE(dev && "No ad9361-phy found");
    return dev;
}

/* finds AD9361 streaming IIO devices */
static bool get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d, struct iio_device **dev)
{
    switch (d)
    {
    case TX:
        *dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
        return *dev != NULL;
    case RX:
        *dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
        return *dev != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

/* finds AD9361 streaming IIO channels */
static bool get_ad9361_stream_ch(enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
    *chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
    if (!*chn)
        *chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
    return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
    switch (d)
    {
    case RX:
        *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), false);
        return *chn != NULL;
    case TX:
        *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), true);
        return *chn != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

/* finds AD9361 local oscillator IIO configuration channels */
static bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn)
{
    switch (d)
    {
        // LO chan is always output, i.e. true
    case RX:
        *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 0), true);
        return *chn != NULL;
    case TX:
        *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 1), true);
        return *chn != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

/* applies streaming configuration through IIO */
bool cfg_ad9361_streaming_ch(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
    struct iio_channel *chn = NULL;

    // Configure phy and lo channels
    printf("* Acquiring AD9361 phy channel %d\n", chid);
    if (!get_phy_chan(ctx, type, chid, &chn))
    {
        return false;
    }
    wr_ch_str(chn, "rf_port_select", cfg->rfport);
    wr_ch_lli(chn, "rf_bandwidth", cfg->bw_hz);
    wr_ch_lli(chn, "sampling_frequency", cfg->fs_hz);

    // Configure LO channel
    printf("* Acquiring AD9361 %s lo channel\n", type == TX ? "TX" : "RX");
    if (!get_lo_chan(ctx, type, &chn))
    {
        return false;
    }
    wr_ch_lli(chn, "frequency", cfg->lo_hz);
    return true;
}

struct pluto_dev pluto_init(const char *ctx_uri, struct stream_cfg txcfg, struct stream_cfg rxcfg)
{
    struct pluto_dev pluto;

    printf("* Acquiring IIO context\n");
    IIO_ENSURE((pluto.ctx = iio_create_context_from_uri(ctx_uri)) && "No context");
    IIO_ENSURE(iio_context_get_devices_count(pluto.ctx) > 0 && "No devices");

    printf("* Acquiring AD9361 streaming devices\n");
    IIO_ENSURE(get_ad9361_stream_dev(pluto.ctx, TX, &pluto.tx) && "No tx dev found");
    IIO_ENSURE(get_ad9361_stream_dev(pluto.ctx, RX, &pluto.rx) && "No rx dev found");

    printf("* Configuring AD9361 for streaming\n");
    IIO_ENSURE(cfg_ad9361_streaming_ch(pluto.ctx, &rxcfg, RX, 0) && "RX port 0 not found");
    IIO_ENSURE(cfg_ad9361_streaming_ch(pluto.ctx, &txcfg, TX, 0) && "TX port 0 not found");

    printf("* Initializing AD9361 IIO streaming channels\n");
    IIO_ENSURE(get_ad9361_stream_ch(RX, pluto.rx, 0, &(pluto.rx0_i)) && "RX chan i not found");
    IIO_ENSURE(get_ad9361_stream_ch(RX, pluto.rx, 1, &(pluto.rx0_q)) && "RX chan q not found");
    IIO_ENSURE(get_ad9361_stream_ch(TX, pluto.tx, 0, &(pluto.tx0_i)) && "TX chan i not found");
    IIO_ENSURE(get_ad9361_stream_ch(TX, pluto.tx, 1, &(pluto.tx0_q)) && "TX chan q not found");

    printf("* Enabling IIO streaming channels\n");
    iio_channel_enable(pluto.rx0_i);
    iio_channel_enable(pluto.rx0_q);
    iio_channel_enable(pluto.tx0_i);
    iio_channel_enable(pluto.tx0_q);

    printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
    pluto.rxbuf = iio_device_create_buffer(pluto.rx, 1024 * 1024, false);
    if (!pluto.rxbuf)
    {
        perror("Could not create RX buffer");
        pluto_shutdown(&pluto);
    }
    pluto.txbuf = iio_device_create_buffer(pluto.tx, 1024 * 1024, false);
    if (!pluto.txbuf)
    {
        perror("Could not create TX buffer");
        pluto_shutdown(&pluto);
    }
    return pluto;
}

size_t pluto_tx(struct pluto_dev *pluto)
{
    ssize_t nbytes_tx;
    char *p_dat, *p_end;
    ptrdiff_t p_inc;

    // Schedule TX buffer
    nbytes_tx = iio_buffer_push(pluto->txbuf);
    if (nbytes_tx < 0)
    {
        printf("Error pushing buf %d\n", (int)nbytes_tx);
        pluto_shutdown(pluto);
    }

    // WRITE: Get pointers to TX buf and write IQ to TX buf port 0
    p_inc = iio_buffer_step(pluto->txbuf);
    p_end = iio_buffer_end(pluto->txbuf);
    for (p_dat = (char *)iio_buffer_first(pluto->txbuf, pluto->tx0_i); p_dat < p_end; p_dat += p_inc)
    {
        // Example: fill with zeros
        // 12-bit sample needs to be MSB aligned so shift by 4
        // https://wiki.analog.com/resources/eval/user-guides/ad-fmcomms2-ebz/software/basic_iq_datafiles#binary_format
        ((int16_t *)p_dat)[0] = 0 << 4; // Real (I)
        ((int16_t *)p_dat)[1] = 0 << 4; // Imag (Q)
    }

    return nbytes_tx / iio_device_get_sample_size(pluto->tx);
}

size_t pluto_rx(struct pluto_dev *pluto)
{
    ssize_t nbytes_rx;
    char *p_dat, *p_end;
    ptrdiff_t p_inc;

    // Refill RX buffer
    nbytes_rx = iio_buffer_refill(pluto->rxbuf);
    if (nbytes_rx < 0)
    {
        printf("Error refilling buf %d\n", (int)nbytes_rx);
        pluto_shutdown(pluto);
    }

    // READ: Get pointers to RX buf and read IQ from RX buf port 0
    p_inc = iio_buffer_step(pluto->rxbuf);
    p_end = iio_buffer_end(pluto->rxbuf);
    for (p_dat = (char *)iio_buffer_first(pluto->rxbuf, pluto->rx0_i); p_dat < p_end; p_dat += p_inc)
    {
        // Example: swap I and Q
        const int16_t i = ((int16_t *)p_dat)[0]; // Real (I)
        const int16_t q = ((int16_t *)p_dat)[1]; // Imag (Q)
        ((int16_t *)p_dat)[0] = q;
        ((int16_t *)p_dat)[1] = i;
    }

    return nbytes_rx / iio_device_get_sample_size(pluto->rx);
}

void pluto_shutdown(struct pluto_dev *pluto)
{
    printf("* Destroying buffers\n");
    if (pluto->rxbuf)
    {
        iio_buffer_destroy(pluto->rxbuf);
    }
    if (pluto->txbuf)
    {
        iio_buffer_destroy(pluto->txbuf);
    }

    printf("* Disabling streaming channels\n");
    if (pluto->rx0_i)
    {
        iio_channel_disable(pluto->rx0_i);
    }
    if (pluto->rx0_q)
    {
        iio_channel_disable(pluto->rx0_q);
    }
    if (pluto->tx0_i)
    {
        iio_channel_disable(pluto->tx0_i);
    }
    if (pluto->tx0_q)
    {
        iio_channel_disable(pluto->tx0_q);
    }

    printf("* Destroying context\n");
    if (pluto->ctx)
    {
        iio_context_destroy(pluto->ctx);
    }
    exit(0);
}