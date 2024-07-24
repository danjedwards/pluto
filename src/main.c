#include <stdio.h>
#include <iio.h>
// #include "iio.h"

int main()
{
    printf("Hello World!\n");
    struct iio_context *ctx = iio_create_context_from_uri("ip:192.168.2.1");
    return 0;
}