#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <argp.h>
#include <zmq.h>
#include <iio.h>

#include "pluto.h"
#include "dsp.h"

/////////////////////////////////////////////////////////////////////////////////////
// argparse
/////////////////////////////////////////////////////////////////////////////////////

const char *argp_program_version = "main 0.1";
const char *argp_program_bug_address = "<my_email>.com";
static char doc[] = "Adalm Pluto and IIO Learning Code";
static char args_doc[] =
    " \
    uri - the context uri.\
    addr - the address to publish program output to. \
    ";

static struct argp_option options[] = {
    {"uri", 'u', "URI", 0, "the context uri."},
    {"addr", 'a', "ADDRESS", 0, "the address to publish program output to."},
    {0}};

struct arguments
{
    char *uri;
    char *addr;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key)
    {
    case 'u':
        arguments->uri = arg;
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

/////////////////////////////////////////////////////////////////////////////////////
// signal handling
/////////////////////////////////////////////////////////////////////////////////////

static bool stop;

static void handle_sig(int sig)
{
    printf("Waiting for process to finish... Got signal %d\n", sig);
    stop = true;
}

/////////////////////////////////////////////////////////////////////////////////////
// main
/////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{

    /////////////////////////////////////////////////////////////////////////////////////
    // argparse
    /////////////////////////////////////////////////////////////////////////////////////

    struct arguments arguments;

    arguments.uri = "ip:pluto.local";
    arguments.addr = "tcp://127.0.0.1:5555";

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    /////////////////////////////////////////////////////////////////////////////////////
    // configure
    /////////////////////////////////////////////////////////////////////////////////////

    signal(SIGINT, handle_sig);

    void *zmq_ctx = zmq_ctx_new();
    void *publisher = zmq_socket(zmq_ctx, ZMQ_PUB);

    int rc = zmq_bind(publisher, arguments.addr);
    if (rc != 0)
    {
        char msg[100] = "Could not bind to the address ";
        if (strlen(arguments.addr) < (sizeof(msg) - strlen(msg) - 1))
            strcat(msg, arguments.addr);
        printf("%s\n", msg);
        exit(-1);
    }

    struct stream_cfg rxcfg, txcfg;

    // RX stream config
    rxcfg.bw_hz = MHZ(2);        // 2 MHz rf bandwidth
    rxcfg.fs_hz = MHZ(2.5);      // 2.5 MS/s rx sample rate
    rxcfg.lo_hz = GHZ(2.5);      // 2.5 GHz rf frequency
    rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

    // TX stream config
    txcfg.bw_hz = MHZ(1.5); // 1.5 MHz rf bandwidth
    txcfg.fs_hz = MHZ(2.5); // 2.5 MS/s tx sample rate
    txcfg.lo_hz = GHZ(2.5); // 2.5 GHz rf frequency
    txcfg.rfport = "A";     // port A (select for rf freq.)

    struct pluto_dev pluto = pluto_init(arguments.uri, txcfg, rxcfg);

    /////////////////////////////////////////////////////////////////////////////////////
    // stream
    /////////////////////////////////////////////////////////////////////////////////////

    size_t ntx = 0, nrx = 0;
    int16_t rx_buf[BUFFER_SIZE * 2], tx_buf[BUFFER_SIZE * 2];

    printf("* Starting IO streaming (press CTRL+C to cancel)\n");
    while (!stop)
    {
        /////////////////////////////////////////////////////////////////////////////////////
        // TX
        /////////////////////////////////////////////////////////////////////////////////////
        // Get buffer length and create buffer of the same size
        ntx = pluto_get_tx_buf_len(&pluto);
        printf("\t transmitting %ld bytes", ntx);

        // Dsp stuff
        int16_sine_wave(txcfg.fs_hz, 50000, tx_buf, ntx / 2); // funky RX signal. Perhaps it is a sampling frequency thing like with the python code??

        // Push bytes to tx buffer
        pluto_tx(&pluto, tx_buf);

        // Send Tx Data over zmq socket
        // if (tx_buf)
        //     zmq_send(publisher, (const void *)tx_buf, ntx, 0);

        /////////////////////////////////////////////////////////////////////////////////////
        // RX
        /////////////////////////////////////////////////////////////////////////////////////
        nrx = pluto_get_rx_buf_len(&pluto);
        printf("\t RX received %ld bytes\n", nrx);

        // Fill buffer
        pluto_rx(&pluto, rx_buf);

        // Dsp stuff

        // Send Rx Data over zmq socket
        if (rx_buf)
            zmq_send(publisher, (const void *)rx_buf, nrx, 0);
    }

    pluto_shutdown(&pluto);

    return 0;
}