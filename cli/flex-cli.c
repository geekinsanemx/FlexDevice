/*
 * FLEX-FSK-TX Binary Protocol Client
 *
 * Command-line tool for communicating with FLEX-FSK-TX firmware via binary protocol.
 * Uses FlexDevice.h library.
 *
 * Usage:
 *   flex-binary-client [OPTIONS] CAPCODE MESSAGE
 *
 * Compile:
 *   gcc -o flex-binary-client flex-binary-client.c -O2 -Wall
 *
 * v2.5.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "libflex_binary/FlexDevice.h"

#define CURRENT_VERSION "v2.5.3"

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS] CAPCODE MESSAGE\n\n", prog);
    printf("Binary Protocol Client for FLEX-FSK-TX %s\n\n", CURRENT_VERSION);
    printf("Options:\n");
    printf("  -d DEVICE    Serial device (default: /dev/ttyUSB0)\n");
    printf("  -b BAUD      Baudrate (default: 115200)\n");
    printf("  -f FREQ      Frequency in MHz (default: 931.9375)\n");
    printf("  -p POWER     TX power in dBm (default: 10)\n");
    printf("  -m           Enable mail drop flag\n");
    printf("  -v           Verbose output\n");
    printf("  -w           Wait for TX_DONE event (up to 30s)\n");
    printf("  -s           Query device status before sending\n");
    printf("  -P           Ping device before sending\n");
    printf("  -h           Show this help\n\n");
    printf("Examples:\n");
    printf("  %s 1234567 \"Hello World\"\n", prog);
    printf("  %s -d /dev/ttyUSB0 -f 931.9375 -p 15 -w 1234567 \"Test\"\n", prog);
    printf("  %s -v -w -m 37137 \"Mail drop message\"\n\n", prog);
}

int main(int argc, char *argv[]) {
    const char *device   = "/dev/ttyUSB0";
    int  baudrate        = 115200;
    float frequency      = 931.9375f;
    int  power           = 10;
    uint8_t mail_drop    = 0;
    int  verbose         = 0;
    int  wait_for_done   = 0;
    int  do_status       = 0;
    int  do_ping         = 0;
    int  opt;

    while ((opt = getopt(argc, argv, "d:b:f:p:mvwsPh")) != -1) {
        switch (opt) {
            case 'd': device    = optarg;         break;
            case 'b': baudrate  = atoi(optarg);   break;
            case 'f': frequency = atof(optarg);   break;
            case 'p': power     = atoi(optarg);   break;
            case 'm': mail_drop = 1;               break;
            case 'v': verbose   = 1;               break;
            case 'w': wait_for_done = 1;           break;
            case 's': do_status = 1;               break;
            case 'P': do_ping   = 1;               break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    if (optind + 2 != argc) {
        fprintf(stderr, "Error: expected CAPCODE and MESSAGE\n\n");
        print_usage(argv[0]);
        return 1;
    }

    uint32_t    capcode = (uint32_t)atoll(argv[optind]);
    const char *message = argv[optind + 1];

    if (verbose) {
        printf("Device:    %s\n", device);
        printf("Baudrate:  %d\n", baudrate);
        printf("Frequency: %.4f MHz\n", frequency);
        printf("Power:     %d dBm\n", power);
        printf("Mail drop: %s\n", mail_drop ? "yes" : "no");
        printf("Capcode:   %u\n", capcode);
        printf("Message:   \"%s\" (%zu bytes)\n\n", message, strlen(message));
    }

    FlexDevice dev;
    if (flex_open(&dev, device, baudrate) < 0) return 1;
    dev.verbose = verbose;

    printf("Connected to %s @ %d baud\n", device, baudrate);

    // Optional ping
    if (do_ping) {
        printf("Pinging device...\n");
        if (flex_ping(&dev) < 0) {
            fprintf(stderr, "Ping failed\n");
            flex_close(&dev);
            return 1;
        }
        printf("Pong received.\n");
    }

    // Optional status query
    if (do_status) {
        printf("Querying device status...\n");
        uint8_t  dev_state = 0, q_count = 0, bat_pct = 0;
        uint16_t bat_mv = 0;
        float    dev_freq = 0.0f;
        int8_t   dev_power = 0;

        if (flex_get_status(&dev, &dev_state, &q_count, &bat_pct, &bat_mv, &dev_freq, &dev_power) == 0) {
            printf("Status: state=%d queue=%d battery=%d%% (%dmV) freq=%.4f MHz power=%d dBm\n",
                   dev_state, q_count, bat_pct, bat_mv, dev_freq, dev_power);
        } else {
            fprintf(stderr, "Status query failed\n");
            flex_close(&dev);
            return 1;
        }
    }

    // Send message
    char uuid_str[37];
    int result;

    printf("Sending message to capcode=%u freq=%.4f MHz power=%d dBm mail_drop=%s\n",
           capcode, frequency, power, mail_drop ? "yes" : "no");

    if (wait_for_done) {
        result = flex_send_msg_wait(&dev, capcode, frequency, (int8_t)power,
                                    mail_drop, message, uuid_str, 30);
    } else {
        result = flex_send_msg(&dev, capcode, frequency, (int8_t)power,
                               mail_drop, message, uuid_str);
        if (result == 0) printf("ACK: Message accepted\n");
    }

    if (result == 0) {
        printf("Done. UUID: %s\n", uuid_str);
    } else {
        fprintf(stderr, "Failed to send message.\n");
        flex_close(&dev);
        return 1;
    }

    flex_close(&dev);
    return 0;
}
