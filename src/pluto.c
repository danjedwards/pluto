#include <stdio.h>
#include <stdbool.h>
#include <iio.h>

#include "pluto.h"

#define IIO_ENSURE(expr)                                                             \
    {                                                                                \
        if (!(expr))                                                                 \
        {                                                                            \
            (void)fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
            (void)abort();                                                           \
        }                                                                            \
    }

/////////////////////////////////////////////////////////////////////////////////////
// Pluto Config/Init
/////////////////////////////////////////////////////////////////////////////////////

static char tmpstr[64]; // static scratch mem for strings

static void errchk(struct pluto_dev *pluto, int v, const char *what)
{
    if (v < 0)
    {
        fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what);
        pluto_shutdown(pluto);
    }
}

static void wr_ch_lli(struct pluto_dev *pluto, struct iio_channel *chn, const char *what, long long val)
{
    errchk(pluto, iio_channel_attr_write_longlong(chn, what, val), what);
}

static void wr_ch_str(struct pluto_dev *pluto, struct iio_channel *chn, const char *what, const char *str)
{
    errchk(pluto, iio_channel_attr_write(chn, what, str), what);
}

static char *get_ch_name(const char *type, int id)
{
    snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
    return tmpstr;
}

static struct iio_device *get_ad9361_phy(struct pluto_dev *pluto)
{
    struct iio_device *dev = iio_context_find_device(pluto->ctx, "ad9361-phy");
    IIO_ENSURE(dev && "No ad9361-phy found");
    return dev;
}

static bool get_pluto_stream_devices(struct pluto_dev *pluto)
{
    pluto->tx = iio_context_find_device(pluto->ctx, "cf-ad9361-dds-core-lpc");
    pluto->rx = iio_context_find_device(pluto->ctx, "cf-ad9361-lpc");
    return (pluto->tx != NULL) & (pluto->rx != NULL);
}

static bool get_ad9361_stream_ch(enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
    *chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
    if (!*chn)
        *chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
    return *chn != NULL;
}

static bool get_phy_chan(struct pluto_dev *pluto, enum iodev d, int chid, struct iio_channel **chn)
{
    switch (d)
    {
    case RX:
        *chn = iio_device_find_channel(get_ad9361_phy(pluto), get_ch_name("voltage", chid), false);
        return *chn != NULL;
    case TX:
        *chn = iio_device_find_channel(get_ad9361_phy(pluto), get_ch_name("voltage", chid), true);
        return *chn != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

static bool get_lo_chan(struct pluto_dev *pluto, enum iodev d, struct iio_channel **chn)
{
    switch (d)
    {
        // LO chan is always output, i.e. true
    case RX:
        *chn = iio_device_find_channel(get_ad9361_phy(pluto), get_ch_name("altvoltage", 0), true);
        return *chn != NULL;
    case TX:
        *chn = iio_device_find_channel(get_ad9361_phy(pluto), get_ch_name("altvoltage", 1), true);
        return *chn != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

bool cfg_pluto_streaming_ch(struct pluto_dev *pluto, struct stream_cfg *cfg, enum iodev type, int chid)
{
    struct iio_channel *chn = NULL;

    // Configure phy and lo channels
    printf("* Acquiring pluto phy channel %d\n", chid);
    if (!get_phy_chan(pluto, type, chid, &chn))
    {
        return false;
    }
    wr_ch_str(pluto, chn, "rf_port_select", cfg->rfport);
    wr_ch_lli(pluto, chn, "rf_bandwidth", cfg->bw_hz);
    wr_ch_lli(pluto, chn, "sampling_frequency", cfg->fs_hz);

    // Configure LO channel
    printf("* Acquiring pluto %s lo channel\n", type == TX ? "TX" : "RX");
    if (!get_lo_chan(pluto, type, &chn))
    {
        return false;
    }
    wr_ch_lli(pluto, chn, "frequency", cfg->lo_hz);
    return true;
}

struct pluto_dev pluto_init(const char *ctx_uri, struct stream_cfg txcfg, struct stream_cfg rxcfg)
{
    struct pluto_dev pluto;

    printf("* Acquiring IIO context\n");
    IIO_ENSURE((pluto.ctx = iio_create_context_from_uri(ctx_uri)) && "No context");
    IIO_ENSURE(iio_context_get_devices_count(pluto.ctx) > 0 && "No devices");

    printf("* Acquiring AD9361 streaming devices\n");
    IIO_ENSURE(get_pluto_stream_devices(&pluto) && "No tx dev found");

    printf("* Configuring AD9361 for streaming\n");
    IIO_ENSURE(cfg_pluto_streaming_ch(&pluto, &rxcfg, RX, 0) && "RX port 0 not found");
    IIO_ENSURE(cfg_pluto_streaming_ch(&pluto, &txcfg, TX, 0) && "TX port 0 not found");

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
    pluto.rxbuf = iio_device_create_buffer(pluto.rx, BUFFER_SIZE, false);
    if (!pluto.rxbuf)
    {
        perror("Could not create RX buffer");
        pluto_shutdown(&pluto);
    }
    pluto.txbuf = iio_device_create_buffer(pluto.tx, BUFFER_SIZE, false);
    if (!pluto.txbuf)
    {
        perror("Could not create TX buffer");
        pluto_shutdown(&pluto);
    }
    return pluto;
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

/////////////////////////////////////////////////////////////////////////////////////
// Pluto Config/Init
/////////////////////////////////////////////////////////////////////////////////////

size_t pluto_get_tx_buf_len(struct pluto_dev *pluto)
{
    ssize_t nbytes_tx;

    // Schedule TX buffer
    nbytes_tx = iio_buffer_push(pluto->txbuf);
    if (nbytes_tx < 0)
    {
        printf("Error pushing buf %d\n", (int)nbytes_tx);
        pluto_shutdown(pluto);
    }

    return nbytes_tx;
}

void pluto_tx(struct pluto_dev *pluto, int16_t *buf)
{
    char *p_dat, *p_end;
    ptrdiff_t p_inc;
    int idx = 0;

    // WRITE: Get pointers to TX buf and write IQ to TX buf port 0
    p_inc = iio_buffer_step(pluto->txbuf);
    p_end = iio_buffer_end(pluto->txbuf);
    for (p_dat = (char *)iio_buffer_first(pluto->txbuf, pluto->tx0_i); p_dat < p_end; p_dat += p_inc)
    {
        // 12-bit sample needs to be MSB aligned so shift by 4
        // https://wiki.analog.com/resources/eval/user-guides/ad-fmcomms2-ebz/software/basic_iq_datafiles#binary_format
        ((int16_t *)p_dat)[0] = buf[idx * 2] << 4;     // Real (I)
        ((int16_t *)p_dat)[1] = buf[idx * 2 + 1] << 4; // Imag (Q)
    }
}

size_t pluto_get_rx_buf_len(struct pluto_dev *pluto)
{
    ssize_t nbytes_rx;

    // Refill RX buffer
    nbytes_rx = iio_buffer_refill(pluto->rxbuf);
    if (nbytes_rx < 0)
    {
        printf("Error refilling buf %d\n", (int)nbytes_rx);
        pluto_shutdown(pluto);
    }

    return nbytes_rx;
}

void pluto_rx(struct pluto_dev *pluto, int16_t *buf)
{
    char *p_dat, *p_end;
    ptrdiff_t p_inc;
    int idx = 0;

    // READ: Get pointers to RX buf and read IQ from RX buf port 0
    p_inc = iio_buffer_step(pluto->rxbuf);
    p_end = iio_buffer_end(pluto->rxbuf);
    for (p_dat = (char *)iio_buffer_first(pluto->rxbuf, pluto->rx0_i); p_dat < p_end; p_dat += p_inc)
    {
        buf[idx * 2] = ((int16_t *)p_dat)[0];     // Real (I)
        buf[idx * 2 + 1] = ((int16_t *)p_dat)[1]; // Imag (Q)
        idx++;
    }
}