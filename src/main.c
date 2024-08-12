#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iio/iio.h>
#include <zmq.h>
#include <assert.h>
#include <argp.h>
#include <signal.h>
// #include <iio/iio-debug.h>

/* helper macros */
#define MHZ(x) ((long long)(x * 1000000.0 + .5))
#define GHZ(x) ((long long)(x * 1000000000.0 + .5))

#define IIO_ENSURE(expr)                                                             \
    {                                                                                \
        if (!(expr))                                                                 \
        {                                                                            \
            (void)fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
            (void)abort();                                                           \
        }                                                                            \
    }

#define BLOCK_SIZE (1024 * 1024)

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

/* IIO structs required for streaming */
static struct iio_context *ctx = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *rx0_q = NULL;
static struct iio_channel *tx0_i = NULL;
static struct iio_channel *tx0_q = NULL;
static struct iio_buffer *rxbuf = NULL;
static struct iio_buffer *txbuf = NULL;
static struct iio_stream *rxstream = NULL;
static struct iio_stream *txstream = NULL;
static struct iio_channels_mask *rxmask = NULL;
static struct iio_channels_mask *txmask = NULL;

static bool stop = false;

void stop_stream(void)
{
    stop = true;
}

void stream(size_t rx_sample, size_t tx_sample, size_t block_size,
            struct iio_stream *rxstream, struct iio_stream *txstream,
            const struct iio_channel *rxchn, const struct iio_channel *txchn)
{
    const struct iio_device *dev;
    const struct iio_context *ctx;
    const struct iio_block *txblock, *rxblock;
    ssize_t nrx = 0;
    ssize_t ntx = 0;
    int err;

    dev = iio_channel_get_device(rxchn);
    ctx = iio_device_get_context(dev);

    while (!stop)
    {
        int16_t *p_dat, *p_end;
        ptrdiff_t p_inc;

        rxblock = iio_stream_get_next_block(rxstream);
        err = iio_err(rxblock);
        if (err)
        {
            ctx_perror(ctx, err, "Unable to receive block");
            return;
        }

        txblock = iio_stream_get_next_block(txstream);
        err = iio_err(txblock);
        if (err)
        {
            ctx_perror(ctx, err, "Unable to send block");
            return;
        }

        /* READ: Get pointers to RX buf and read IQ from RX buf port 0 */
        p_inc = rx_sample;
        p_end = iio_block_end(rxblock);
        for (p_dat = iio_block_first(rxblock, rxchn); p_dat < p_end;
             p_dat += p_inc / sizeof(*p_dat))
        {
            /* Example: swap I and Q */
            int16_t i = p_dat[0];
            int16_t q = p_dat[1];

            p_dat[0] = q;
            p_dat[1] = i;
        }

        /* WRITE: Get pointers to TX buf and write IQ to TX buf port 0 */
        p_inc = tx_sample;
        p_end = iio_block_end(txblock);
        for (p_dat = iio_block_first(txblock, txchn); p_dat < p_end;
             p_dat += p_inc / sizeof(*p_dat))
        {
            p_dat[0] = 0; /* Real (I) */
            p_dat[1] = 0; /* Imag (Q) */
        }

        nrx += block_size / rx_sample;
        ntx += block_size / tx_sample;
        ctx_info(ctx, "\tRX %8.2f MSmp, TX %8.2f MSmp\n", nrx / 1e6, ntx / 1e6);
    }
}

static void shutdown()
{
    printf("* Destroying streams\n");
    if (ctx)
        iio_context_destroy(ctx);
    exit(0);
}

static void handle_sig(int sig)
{
    printf("Waiting for process to finish... Got signal %d\n", sig);
    stop_stream();
}

/* check return value of attr_write function */
static void errchk(int v, const char *what)
{
    if (v < 0)
    {
        fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what);
        shutdown();
    }
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char *what, long long val)
{
    const struct iio_attr *attr = iio_channel_find_attr(chn, what);

    errchk(attr ? iio_attr_write_longlong(attr, val) : -ENOENT, what);
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char *what, const char *str)
{
    const struct iio_attr *attr = iio_channel_find_attr(chn, what);

    errchk(attr ? iio_attr_write_string(attr, str) : -ENOENT, what);
}

static struct iio_device *get_pluto_phy(void)
{
    struct iio_device *dev = iio_context_find_device(ctx, "ad9361-phy");
    IIO_ENSURE(dev && "No ad9361-phy found");
    return dev;
}

static bool get_pluto_stream_dev(enum iodev d, struct iio_device **dev)
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

static bool get_pluto_phy_chan(enum iodev d, int chid, struct iio_channel **chn)
{
    switch (d)
    {
    case RX:
        *chn = iio_device_find_channel(get_ad9361_phy(), get_ch_name("voltage", chid), false);
        return *chn != NULL;
    case TX:
        *chn = iio_device_find_channel(get_ad9361_phy(), get_ch_name("voltage", chid), true);
        return *chn != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

static bool get_pluto_stream_ch(enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
    *chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
    if (!*chn)
        *chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
    return *chn != NULL;
}

static bool get_lo_chan(enum iodev d, struct iio_channel **chn)
{
    switch (d)
    {
        // LO chan is always output, i.e. true
    case RX:
        *chn = iio_device_find_channel(get_ad9361_phy(), get_ch_name("altvoltage", 0), true);
        return *chn != NULL;
    case TX:
        *chn = iio_device_find_channel(get_ad9361_phy(), get_ch_name("altvoltage", 1), true);
        return *chn != NULL;
    default:
        IIO_ENSURE(0);
        return false;
    }
}

bool cfg_pluto_streaming_ch(struct stream_cfg *cfg, enum iodev type, int chid)
{
    const struct iio_attr *attr;
    struct iio_channel *chn = NULL;

    // Configure phy and lo channels
    printf("* Acquiring AD9361 phy channel %d\n", chid);
    if (!get_phy_chan(type, chid, &chn))
    {
        return false;
    }

    attr = iio_channel_find_attr(chn, "rf_port_select");
    if (attr)
        errchk(iio_attr_write_string(attr, cfg->rfport), cfg->rfport);
    wr_ch_lli(chn, "rf_bandwidth", cfg->bw_hz);
    wr_ch_lli(chn, "sampling_frequency", cfg->fs_hz);

    // Configure LO channel
    printf("* Acquiring AD9361 %s lo channel\n", type == TX ? "TX" : "RX");
    if (!get_lo_chan(type, &chn))
    {
        return false;
    }
    wr_ch_lli(chn, "frequency", cfg->lo_hz);
    return true;
}
/*
iio:device0: ad9361-phy
iio:device1: xadc
iio:device2: one-bit-adc-dac
iio:device3: cf-ad9361-dds-core-lpc (buffer capable) => dis one
iio:device4: cf-ad9361-lpc (buffer capable) => dis one
iio:device5: adi-iio-fakedev (label: iio-axi-tdd-0)
*/

int main(int argc, char **argv)
{

    // Streaming devices
    struct iio_device *tx;
    struct iio_device *rx;

    // RX and TX sample counters
    size_t nrx = 0;
    size_t ntx = 0;

    // RX and TX sample size
    size_t rx_sample_sz, tx_sample_sz;

    // Stream configurations
    struct stream_cfg rxcfg;
    struct stream_cfg txcfg;

    int err;

    // Listen to ctrl+c and II_ENSURE
    signal(SIGINT, handle_sig);

    // TX stream config
    txcfg.bw_hz = MHZ(1.5);
    txcfg.fs_hz = MHZ(2.5);
    txcfg.lo_hz = GHZ(2.5);
    txcfg.rfport = "A";

    printf("* Acquiring IIO context\n");
    if (argc == 1)
    {
        IIO_ENSURE((ctx = iio_create_context(NULL, NULL)) && "No context");
    }
    else if (argc == 2)
    {
        IIO_ENSURE((ctx = iio_create_context(NULL, argv[1])) && "No context");
    }
    IIO_ENSURE(iio_context_get_devices_count(ctx) > 0 && "No devices");

    printf("* Acquiring Pluto streaming devices\n");
    IIO_ENSURE(get_pluto_stream_dev(TX, &tx) && "No tx dev found");
    IIO_ENSURE(get_pluto_stream_dev(RX, &rx) && "No rx dev found");

    printf("* Configuring Pluto for streaming\n");
    IIO_ENSURE(cfg_pluto_streaming_ch(&txcfg, TX, 0) && "TX port 0 not found");
    IIO_ENSURE(cfg_pluto_streaming_ch(&rxcfg, RX, 0) && "RX port 0 not found");

    printf("* Initialising Pluto IIO streaming channels\n");

    IIO_ENSURE(get_pluto_stream_ch(TX, tx, 0, &tx0_i) && "TX chan i not found");
    IIO_ENSURE(get_pluto_stream_ch(TX, tx, 1, &tx0_q) && "TX chan q not found");
    IIO_ENSURE(get_pluto_stream_ch(RX, rx, 0, &rx0_i) && "RX chan i not found");
    IIO_ENSURE(get_pluto_stream_ch(RX, rx, 1, &rx0_q) && "RX chan q not found");

    rxmask = iio_create_channels_mask(iio_device_get_channels_count(rx));
    if (!rxmask)
    {
        fprintf(stderr, "Unable to alloc channels mask\n");
        shutdown();
    }

    txmask = iio_create_channels_mask(iio_device_get_channels_count(tx));
    if (!txmask)
    {
        fprintf(stderr, "Unable to alloc channels mask\n");
        shutdown();
    }

    printf("* Enabling IIO streaming channels\n");
    iio_channel_enable(rx0_i, rxmask);
    iio_channel_enable(rx0_q, rxmask);
    iio_channel_enable(tx0_i, txmask);
    iio_channel_enable(tx0_q, txmask);

    printf("* Creating non-cyclic IIO buffers with 1 MiS\n");
    rxbuf = iio_device_create_buffer(rx, 0, rxmask);
    err = iio_err(rxbuf);
    if (err)
    {
        rxbuf = NULL;
        dev_perror(rx, err, "Could not create RX buffer");
        shutdown();
    }

    txbuf = iio_device_create_buffer(tx, 0, txmask);
    err = iio_err(txbuf);
    if (err)
    {
        txbuf = NULL;
        dev_perror(tx, err, "Could not create TX buffer");
        shutdown();
    }

    rxstream = iio_buffer_create_stream(rxbuf, 4, BLOCK_SIZE);
    err = iio_err(rxstream);
    if (err)
    {
        rxstream = NULL;
        dev_perror(rx, iio_err(rxstream), "Could not create RX stream");
        shutdown();
    }

    txstream = iio_buffer_create_stream(txbuf, 4, BLOCK_SIZE);
    err = iio_err(txstream);
    if (err)
    {
        txstream = NULL;
        dev_perror(tx, iio_err(txstream), "Could not create TX stream");
        shutdown();
    }

    rx_sample_sz = iio_device_get_sample_size(rx, rxmask);
    tx_sample_sz = iio_device_get_sample_size(tx, txmask);

    printf("* Starting IO streaming (press CTRL+C to cancel)\n");
    stream(rx_sample_sz, tx_sample_sz, BLOCK_SIZE,
           rxstream, txstream, rx0_i, tx0_i);

    shutdown();

    return 0;
}

#if 0
const char *argp_program_version = "main 0.1";
const char *argp_program_bug_address = "<my_email>.com";
static char doc[] = "Adalm Pluto and IIO Learning Code";
static char args_doc[] =
    "addr - the address to publish program output to. \
    ";

static struct argp_option options[] = {
    {"addr", 'a', "ADDRESS", 0, "the address to publish program output to."},
    {0}};

struct arguments
{
    char *addr;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key)
    {
    case 'a':
        arguments->addr = arg;
        break;
        // more cases ...

    case ARGP_KEY_ARG:
        // Use case for required positional arguments
        argp_usage(state);
        break;

    case ARGP_KEY_END:
        /* // Use case for required positional arguments
        if (state->arg_num < 2)
            // Not enough arguments.
            argp_usage(state);
        */
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

int main(int argc, char *argv[])
{
    // Parse Args
    struct arguments arguments;

    arguments.addr = "tcp://127.0.0.1:5555";

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // Set up zmq publisher
    void *zmq_ctx = zmq_ctx_new();
    void *publisher = zmq_socket(zmq_ctx, ZMQ_PUB);

    int rc = zmq_bind(publisher, "tcp://127.0.0.1:5555");
    if (rc != 0)
    {
        char msg[100] = "Could not bind to the address ";
        if (strlen(arguments.addr) < (sizeof(msg) - strlen(msg) - 1))
            strcat(msg, arguments.addr);
        printf("%s\n", msg);
        exit(-1);
    }

    // Publish debug message to zmq socket
    printf("Starting...\n");
    while (true)
    {
        char msg[] = "Hello World\n";
        zmq_send(publisher, msg, strlen(msg), 0);
        sleep(1);
    }
    return 0;
}
#endif