#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iio.h>
#include <zmq.h>
#include <assert.h>
#include <argp.h>

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