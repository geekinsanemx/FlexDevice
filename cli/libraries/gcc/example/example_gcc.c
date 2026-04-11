/*
 * example.c - FlexDevice library usage example
 *
 * Compile:
 *   gcc -o example example.c -O2 -Wall
 *
 * Usage:
 *   ./example /dev/ttyUSB0 1234567 "Hello World"
 *   ./example -f 929.6625 -p 15 -w /dev/ttyACM0 37137 "Test"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "FlexDevice.h"

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS] DEVICE CAPCODE MESSAGE\n\n", prog);
    printf("Options:\n");
    printf("  -b BAUD      Baudrate (default: 115200)\n");
    printf("  -f FREQ      Frequency in MHz (default: 931.9375)\n");
    printf("  -p POWER     TX power in dBm (default: 10)\n");
    printf("  -m           Enable mail drop flag\n");
    printf("  -v           Verbose output\n");
    printf("  -w           Wait for TX_DONE event\n");
    printf("  -R           Toggle DTR/RTS before sending\n");
    printf("  -h           Show this help\n\n");
    printf("Examples:\n");
    printf("  %s /dev/ttyUSB0 1234567 \"Hello World\"\n", prog);
    printf("  %s -f 929.6625 -p 15 -w /dev/ttyACM0 37137 \"Test message\"\n", prog);
}

int main(int argc, char *argv[]) {
    int     baudrate      = 115200;
    float   frequency     = 931.9375f;
    int     power         = 10;
    uint8_t mail_drop     = 0;
    int     verbose       = 0;
    int     wait_for_done = 0;
    int     reset_lines   = 0;
    int     opt;

    while ((opt = getopt(argc, argv, "b:f:p:mvwRh")) != -1) {
        switch (opt) {
            case 'b': baudrate  = atoi(optarg);   break;
            case 'f': frequency = atof(optarg);   break;
            case 'p': power     = atoi(optarg);   break;
            case 'm': mail_drop = 1;               break;
            case 'v': verbose   = 1;               break;
            case 'w': wait_for_done = 1;           break;
            case 'R': reset_lines = 1;             break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    if (optind + 3 != argc) {
        fprintf(stderr, "Error: expected DEVICE CAPCODE MESSAGE\n\n");
        print_usage(argv[0]);
        return 1;
    }

    const char *device  = argv[optind];
    uint64_t    capcode = (uint64_t)strtoull(argv[optind + 1], NULL, 10);
    const char *message = argv[optind + 2];

    FlexDevice dev;
    if (flex_open(&dev, device, baudrate) < 0) return 1;
    dev.verbose = verbose;

    if (reset_lines) {
        flex_reset_lines(&dev, 100);
        if (verbose) printf("UART control lines toggled (DTR low/high, RTS pulse)\n");
    }

    printf("Connected to %s @ %d baud\n", device, baudrate);
    printf("Capcode: %llu  Freq: %.4f MHz  Power: %d dBm  Mail drop: %s\n",
           (unsigned long long)capcode, frequency, power, mail_drop ? "yes" : "no");
    printf("Message: \"%s\" (%zu bytes)\n\n", message, strlen(message));

    char uuid_str[37];
    int result;

    if (wait_for_done) {
        result = flex_send_msg_wait(&dev, capcode, frequency, (int8_t)power,
                                    mail_drop, message, uuid_str, 30);
    } else {
        result = flex_send_msg(&dev, capcode, frequency, (int8_t)power,
                               mail_drop, message, uuid_str);
    }

    if (result == 0) {
        printf("Message sent. UUID: %s\n", uuid_str);
    } else {
        fprintf(stderr, "Failed to send message.\n");
        flex_close(&dev);
        return 1;
    }

    flex_close(&dev);
    return 0;
}
