/*
 * FlexDevice.h - FLEX Binary Protocol Client Library (Header-Only)
 *
 * Single-header C library for communicating with FLEX-FSK-TX firmware
 * via binary COBS-framed protocol over UART.
 *
 * Usage:
 *   #include "FlexDevice.h"
 *
 *   FlexDevice dev;
 *   if (flex_open(&dev, "/dev/ttyUSB0", 115200) == 0) {
 *       char uuid[37];
 *       flex_send_msg(&dev, 1234567, 931.9375, 10, 0, "Hello World", uuid);
 *       flex_close(&dev);
 *   }
 *
 * Protocol reference: binary_packet.h in firmware source
 */

#ifndef FLEXDEVICE_H
#define FLEXDEVICE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

// =============================================================================
// PROTOCOL CONSTANTS  (must match binary_packet.h in firmware)
// =============================================================================

#define FLEX_PACKET_SIZE      512
#define FLEX_HEADER_SIZE       22
#define FLEX_PAYLOAD_SIZE     480
#define FLEX_CRC_OFFSET       510
#define FLEX_TS_OFFSET        502
#define FLEX_MAX_MESSAGE      248
#define FLEX_MAX_MSG_PROTO    255

// Packet types
#define FLEX_PKT_CMD   0x01
#define FLEX_PKT_RSP   0x02
#define FLEX_PKT_EVT   0x03

// Commands
#define FLEX_CMD_SEND_FLEX     0x01
#define FLEX_CMD_GET_STATUS    0x02
#define FLEX_CMD_PING          0x06

// Responses
#define FLEX_RSP_ACK     0x01
#define FLEX_RSP_NACK    0x02
#define FLEX_RSP_STATUS  0x03
#define FLEX_RSP_PONG    0x05

// Events
#define FLEX_EVT_TX_QUEUED  0x01
#define FLEX_EVT_TX_START   0x02
#define FLEX_EVT_TX_DONE    0x03
#define FLEX_EVT_TX_FAILED  0x04

// Flags
#define FLEX_FLAG_ACK_REQUIRED 0x01

// Status codes (ACK payload[0])
#define FLEX_STATUS_ACCEPTED      0x00
#define FLEX_STATUS_REJECTED      0x01
#define FLEX_STATUS_QUEUE_FULL    0x02
#define FLEX_STATUS_INVALID_PARAM 0x03
#define FLEX_STATUS_BUSY          0x04
#define FLEX_STATUS_ERROR         0x05

// Result codes (EVT_TX_DONE/FAILED payload[0])
#define FLEX_RESULT_SUCCESS        0x00
#define FLEX_RESULT_RADIO_ERROR    0x01
#define FLEX_RESULT_ENCODING_ERROR 0x02
#define FLEX_RESULT_TIMEOUT        0x03
#define FLEX_RESULT_ABORTED        0x04

// Timestamp flags
#define FLEX_TS_VALID       0x01
#define FLEX_TS_AUTO_ADJUST 0x02
#define FLEX_TS_SYNC_RTC    0x04
#define FLEX_TS_DST_ACTIVE  0x08

// CMD_SEND_FLEX payload layout:
// [0-7]   capcode   (8 bytes, little-endian uint64)
// [8-11]  frequency (4 bytes, IEEE 754 float, little-endian)
// [12]    tx_power  (1 byte, int8)
// [13]    mail_drop (1 byte, 0/1)
// [14]    msg_len   (1 byte)
// [15+]   message   (msg_len bytes)
#define FLEX_CMD_SEND_ARGS_SIZE 15

// =============================================================================
// PUBLIC TYPES
// =============================================================================

typedef struct {
    int      fd;
    uint8_t  seq;
    int      verbose;
} FlexDevice;

typedef struct {
    uint8_t  pkt_type;
    uint8_t  opcode;
    uint8_t  flags;
    uint8_t  seq;
    uint8_t  uuid[16];
    uint16_t payload_len;
    uint8_t  payload[FLEX_PAYLOAD_SIZE];
    uint32_t ts_unix;
    uint16_t ts_ms;
    int8_t   ts_tz;
    uint8_t  ts_flags;
    uint16_t crc16;
} flex_packet_t;

// =============================================================================
// CRC16-CCITT
// =============================================================================

static const uint16_t _flex_crc_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static inline uint16_t _flex_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (uint16_t)((crc << 8) ^ _flex_crc_table[((crc >> 8) ^ data[i]) & 0xFF]);
    }
    return crc;
}

// =============================================================================
// COBS
// =============================================================================

static inline size_t _flex_cobs_encode(const uint8_t *input, size_t length, uint8_t *output) {
    const uint8_t *src = input;
    uint8_t *dst = output;
    uint8_t *code_ptr = dst++;
    uint8_t code = 0x01;

    for (size_t i = 0; i < length; i++) {
        if (*src == 0x00) {
            *code_ptr = code;
            code_ptr = dst++;
            code = 0x01;
        } else {
            *dst++ = *src;
            code++;
            if (code == 0xFF) {
                *code_ptr = code;
                code_ptr = dst++;
                code = 0x01;
            }
        }
        src++;
    }
    *code_ptr = code;
    *dst++ = 0x00;
    return (size_t)(dst - output);
}

static inline size_t _flex_cobs_decode(const uint8_t *input, size_t length, uint8_t *output) {
    if (length == 0 || input[length - 1] != 0x00) return 0;
    const uint8_t *src = input;
    uint8_t *dst = output;
    size_t remaining = length - 1;

    while (remaining > 0) {
        uint8_t code = *src++;
        remaining--;
        if (code == 0x00) return 0;
        for (uint8_t i = 1; i < code && remaining > 0; i++) {
            *dst++ = *src++;
            remaining--;
        }
        if (code < 0xFF && remaining > 0) {
            *dst++ = 0x00;
        }
    }
    return (size_t)(dst - output);
}

// =============================================================================
// UUID v4 using /dev/urandom
// =============================================================================

static inline void _flex_uuid_v4(uint8_t uuid[16]) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(uuid, 16, 1, f) != 1) memset(uuid, 0, 16);
        fclose(f);
        uuid[6] = (uuid[6] & 0x0F) | 0x40;
        uuid[8] = (uuid[8] & 0x3F) | 0x80;
    } else {
        memset(uuid, 0, 16);
    }
}

static inline void _flex_uuid_to_str(const uint8_t uuid[16], char str[37]) {
    snprintf(str, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0],  uuid[1],  uuid[2],  uuid[3],
        uuid[4],  uuid[5],  uuid[6],  uuid[7],
        uuid[8],  uuid[9],  uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
}

// =============================================================================
// ENDIANNESS HELPERS
// =============================================================================

static inline uint16_t _flex_htons(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static inline uint16_t _flex_ntohs(uint16_t v) { return _flex_htons(v); }
static inline uint32_t _flex_htonl(uint32_t v) {
    return ((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8) |
           ((v & 0x0000FF00) << 8)  | ((v & 0x000000FF) << 24);
}
static inline uint32_t _flex_ntohl(uint32_t v) { return _flex_htonl(v); }

// =============================================================================
// SERIAL PORT
// =============================================================================

static inline int flex_open(FlexDevice *dev, const char *device, int baudrate) {
    dev->fd = -1;
    dev->seq = 1;
    dev->verbose = 0;

    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "flex_open: failed to open %s: %s\n", device, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "flex_open: tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    speed_t speed;
    switch (baudrate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        default:
            fprintf(stderr, "flex_open: unsupported baudrate %d\n", baudrate);
            close(fd);
            return -1;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "flex_open: tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    tcflush(fd, TCIOFLUSH);

    dev->fd = fd;
    return 0;
}

static inline void flex_close(FlexDevice *dev) {
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}

// =============================================================================
// INTERNAL: READ ONE COBS FRAME
// Drains and prints ASCII lines from device; returns binary frame length on
// 0x00 delimiter, -1 on timeout.
// =============================================================================

static inline int _flex_read_frame(FlexDevice *dev, uint8_t *buf, size_t max_len, int timeout_ms) {
    uint8_t byte;
    size_t  pos = 0;
    char    ascii_line[512];
    bool    ascii_candidate = true;

    struct timeval start, now;
    gettimeofday(&start, NULL);

    while (1) {
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec  - start.tv_sec)  * 1000 +
                       (now.tv_usec - start.tv_usec) / 1000;
        if (elapsed > timeout_ms) return -1;

        ssize_t n = read(dev->fd, &byte, 1);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { usleep(1000); continue; }
            return -1;
        }
        if (n == 0) { usleep(1000); continue; }

        if (ascii_candidate && byte == '\r') continue;

        if (pos < max_len) buf[pos++] = byte;

        if (ascii_candidate) {
            if (byte == '\n') {
                size_t line_len = pos - 1;
                if (line_len > 0) {
                    size_t copy = (line_len < sizeof(ascii_line) - 1) ? line_len : sizeof(ascii_line) - 1;
                    memcpy(ascii_line, buf, copy);
                    ascii_line[copy] = '\0';
                    printf("DEVICE: %s\n", ascii_line);
                }
                pos = 0;
                continue;
            }
            if (byte < 0x20 || byte > 0x7E) {
                ascii_candidate = false;
            } else {
                continue;
            }
        }

        if (byte == 0x00) {
            if (dev->verbose) {
                printf("RX [%zu COBS bytes]: ", pos);
                for (size_t i = 0; i < pos && i < 32; i++) printf("%02X ", buf[i]);
                if (pos > 32) printf("...");
                printf("\n");
            }
            return (int)pos;
        }
    }
}

// =============================================================================
// INTERNAL: BUILD RAW 512-BYTE PACKET
// =============================================================================

static inline void _flex_populate_ts(uint8_t *raw) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now_sec = tv.tv_sec;
    struct tm *lt  = localtime(&now_sec);
    int8_t tz_units = (int8_t)(lt->tm_gmtoff / 1800);

    uint32_t ts_be = _flex_htonl((uint32_t)tv.tv_sec);
    uint16_t ms_be = _flex_htons((uint16_t)(tv.tv_usec / 1000));
    memcpy(raw + FLEX_TS_OFFSET,     &ts_be,   4);
    memcpy(raw + FLEX_TS_OFFSET + 4, &ms_be,   2);
    raw[FLEX_TS_OFFSET + 6] = (uint8_t)tz_units;
    raw[FLEX_TS_OFFSET + 7] = FLEX_TS_VALID | FLEX_TS_AUTO_ADJUST | FLEX_TS_SYNC_RTC;
}

static inline void _flex_build_cmd_send_flex(uint8_t raw[FLEX_PACKET_SIZE],
                                              uint8_t seq, const uint8_t uuid[16],
                                              uint64_t capcode, float frequency,
                                              int8_t power, uint8_t mail_drop,
                                              const char *message, uint8_t msg_len) {
    memset(raw, 0, FLEX_PACKET_SIZE);

    raw[0] = FLEX_PKT_CMD;
    raw[1] = FLEX_CMD_SEND_FLEX;
    raw[2] = FLEX_FLAG_ACK_REQUIRED;
    raw[3] = seq;
    memcpy(raw + 4, uuid, 16);

    uint16_t plen_be = _flex_htons((uint16_t)(FLEX_CMD_SEND_ARGS_SIZE + msg_len));
    memcpy(raw + 20, &plen_be, 2);

    // payload starts at offset 22
    memcpy(raw + 22, &capcode,   8);  // little-endian uint64 as-is
    memcpy(raw + 30, &frequency, 4);  // IEEE 754 float as-is (little-endian host)
    raw[34] = (uint8_t)power;
    raw[35] = mail_drop;
    raw[36] = msg_len;
    memcpy(raw + 37, message, msg_len);

    _flex_populate_ts(raw);

    uint16_t crc = _flex_crc16(raw, FLEX_CRC_OFFSET);
    memcpy(raw + FLEX_CRC_OFFSET, &crc, 2);
}

static inline void _flex_build_cmd_ping(uint8_t raw[FLEX_PACKET_SIZE],
                                         uint8_t seq, const uint8_t uuid[16]) {
    memset(raw, 0, FLEX_PACKET_SIZE);
    raw[0] = FLEX_PKT_CMD;
    raw[1] = FLEX_CMD_PING;
    raw[2] = FLEX_FLAG_ACK_REQUIRED;
    raw[3] = seq;
    memcpy(raw + 4, uuid, 16);
    uint16_t plen_be = _flex_htons(0);
    memcpy(raw + 20, &plen_be, 2);
    _flex_populate_ts(raw);
    uint16_t crc = _flex_crc16(raw, FLEX_CRC_OFFSET);
    memcpy(raw + FLEX_CRC_OFFSET, &crc, 2);
}

static inline void _flex_build_cmd_get_status(uint8_t raw[FLEX_PACKET_SIZE],
                                               uint8_t seq, const uint8_t uuid[16]) {
    memset(raw, 0, FLEX_PACKET_SIZE);
    raw[0] = FLEX_PKT_CMD;
    raw[1] = FLEX_CMD_GET_STATUS;
    raw[2] = FLEX_FLAG_ACK_REQUIRED;
    raw[3] = seq;
    memcpy(raw + 4, uuid, 16);
    uint16_t plen_be = _flex_htons(0);
    memcpy(raw + 20, &plen_be, 2);
    _flex_populate_ts(raw);
    uint16_t crc = _flex_crc16(raw, FLEX_CRC_OFFSET);
    memcpy(raw + FLEX_CRC_OFFSET, &crc, 2);
}

// =============================================================================
// INTERNAL: SEND RAW PACKET
// =============================================================================

static inline int _flex_send_raw(FlexDevice *dev, const uint8_t raw[FLEX_PACKET_SIZE]) {
    uint8_t cobs[FLEX_PACKET_SIZE + 10];
    size_t cobs_len = _flex_cobs_encode(raw, FLEX_PACKET_SIZE, cobs);

    if (dev->verbose) {
        char uuid_str[37];
        _flex_uuid_to_str(raw + 4, uuid_str);
        printf("TX [uuid=%s, %zu COBS bytes]: ", uuid_str, cobs_len);
        for (size_t i = 0; i < cobs_len && i < 32; i++) printf("%02X ", cobs[i]);
        if (cobs_len > 32) printf("...");
        printf("\n");
    }

    ssize_t written = write(dev->fd, cobs, cobs_len);
    if (written < 0 || (size_t)written != cobs_len) {
        fprintf(stderr, "flex: write error: %s\n", strerror(errno));
        return -1;
    }
    fsync(dev->fd);
    return 0;
}

// =============================================================================
// INTERNAL: RECEIVE AND DECODE ONE PACKET
// =============================================================================

static inline int _flex_recv_packet(FlexDevice *dev, flex_packet_t *pkt, int timeout_ms) {
    uint8_t frame[FLEX_PACKET_SIZE + 10];
    int frame_len = _flex_read_frame(dev, frame, sizeof(frame), timeout_ms);
    if (frame_len < 0) return -1;

    uint8_t decoded[FLEX_PACKET_SIZE];
    size_t  decoded_len = _flex_cobs_decode(frame, (size_t)frame_len, decoded);
    if (decoded_len != FLEX_PACKET_SIZE) {
        fprintf(stderr, "flex: invalid decoded size %zu (expected %d)\n",
                decoded_len, FLEX_PACKET_SIZE);
        return -1;
    }

    uint16_t calc_crc;
    memcpy(&calc_crc, decoded + FLEX_CRC_OFFSET, 2);
    uint16_t exp_crc = _flex_crc16(decoded, FLEX_CRC_OFFSET);
    if (calc_crc != exp_crc) {
        fprintf(stderr, "flex: CRC mismatch: got 0x%04X expected 0x%04X\n",
                calc_crc, exp_crc);
        return -1;
    }

    pkt->pkt_type    = decoded[0];
    pkt->opcode      = decoded[1];
    pkt->flags       = decoded[2];
    pkt->seq         = decoded[3];
    memcpy(pkt->uuid, decoded + 4, 16);
    memcpy(&pkt->payload_len, decoded + 20, 2);
    pkt->payload_len = _flex_ntohs(pkt->payload_len);
    memcpy(pkt->payload, decoded + 22, FLEX_PAYLOAD_SIZE);

    uint32_t ts_be; memcpy(&ts_be, decoded + FLEX_TS_OFFSET,     4);
    uint16_t ms_be; memcpy(&ms_be, decoded + FLEX_TS_OFFSET + 4, 2);
    pkt->ts_unix  = _flex_ntohl(ts_be);
    pkt->ts_ms    = _flex_ntohs(ms_be);
    pkt->ts_tz    = (int8_t)decoded[FLEX_TS_OFFSET + 6];
    pkt->ts_flags = decoded[FLEX_TS_OFFSET + 7];

    memcpy(&pkt->crc16, decoded + FLEX_CRC_OFFSET, 2);

    if (dev->verbose) {
        char uuid_str[37];
        _flex_uuid_to_str(pkt->uuid, uuid_str);
        printf("PARSED: type=0x%02X opcode=0x%02X seq=%d uuid=%s\n",
               pkt->pkt_type, pkt->opcode, pkt->seq, uuid_str);
        if (pkt->ts_flags & FLEX_TS_VALID) {
            time_t ts = (time_t)pkt->ts_unix;
            char tbuf[64];
            struct tm *tm_info = localtime(&ts);
            if (tm_info) {
                strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);
                printf("ESP32 time: %s.%03d (UTC%+.1f)\n",
                       tbuf, pkt->ts_ms, pkt->ts_tz * 0.5);
            }
        }
    }

    return 0;
}

// =============================================================================
// PUBLIC API
// =============================================================================

/*
 * flex_send_msg - Send a FLEX message and wait for ACK.
 *
 * Parameters:
 *   dev       - Opened FlexDevice
 *   capcode   - FLEX capcode
 *   frequency - Frequency in MHz (e.g. 931.9375)
 *   power     - TX power in dBm
 *   mail_drop - 0=normal, 1=mail drop
 *   message   - Null-terminated message string (max 248 chars; truncated if longer)
 *   uuid_out  - Optional: buffer of at least 37 bytes to receive UUID string (or NULL)
 *
 * Returns:
 *   0  on STATUS_ACCEPTED
 *  -1  on error / NACK / timeout
 */
static inline int flex_send_msg(FlexDevice *dev, uint64_t capcode, float frequency,
                                 int8_t power, uint8_t mail_drop, const char *message,
                                 char *uuid_out) {
    uint8_t uuid[16];
    _flex_uuid_v4(uuid);
    if (uuid_out) _flex_uuid_to_str(uuid, uuid_out);

    size_t mlen = strlen(message);
    uint8_t msg_len = (mlen > FLEX_MAX_MSG_PROTO) ? FLEX_MAX_MSG_PROTO : (uint8_t)mlen;

    uint8_t raw[FLEX_PACKET_SIZE];
    _flex_build_cmd_send_flex(raw, dev->seq++, uuid, capcode, frequency,
                              power, mail_drop, message, msg_len);

    if (_flex_send_raw(dev, raw) < 0) return -1;

    flex_packet_t rsp;
    if (_flex_recv_packet(dev, &rsp, 5000) < 0) {
        fprintf(stderr, "flex: timeout waiting for ACK\n");
        return -1;
    }

    if (rsp.pkt_type == FLEX_PKT_RSP && rsp.opcode == FLEX_RSP_ACK) {
        if (memcmp(rsp.uuid, uuid, 16) != 0) {
            char got[37]; _flex_uuid_to_str(rsp.uuid, got);
            char exp[37]; _flex_uuid_to_str(uuid, exp);
            fprintf(stderr, "flex: UUID mismatch in ACK (expected %s got %s)\n", exp, got);
        }
        uint8_t status = rsp.payload[0];
        if (status == FLEX_STATUS_ACCEPTED) return 0;
        fprintf(stderr, "flex: ACK rejected status=0x%02X\n", status);
        return -1;
    }

    if (rsp.pkt_type == FLEX_PKT_RSP && rsp.opcode == FLEX_RSP_NACK) {
        fprintf(stderr, "flex: NACK error=0x%02X\n", rsp.payload[0]);
        return -1;
    }

    fprintf(stderr, "flex: unexpected response type=0x%02X opcode=0x%02X\n",
            rsp.pkt_type, rsp.opcode);
    return -1;
}

/*
 * flex_send_msg_wait - Send FLEX message, wait for ACK, then wait for TX_DONE/TX_FAILED.
 *
 * Returns:
 *   0  on TX_DONE SUCCESS
 *  -1  on error / TX_FAILED / timeout
 */
static inline int flex_send_msg_wait(FlexDevice *dev, uint64_t capcode, float frequency,
                                      int8_t power, uint8_t mail_drop, const char *message,
                                      char *uuid_out, int wait_timeout_sec) {
    uint8_t uuid[16];
    _flex_uuid_v4(uuid);
    if (uuid_out) _flex_uuid_to_str(uuid, uuid_out);

    size_t mlen = strlen(message);
    uint8_t msg_len = (mlen > FLEX_MAX_MSG_PROTO) ? FLEX_MAX_MSG_PROTO : (uint8_t)mlen;

    uint8_t raw[FLEX_PACKET_SIZE];
    _flex_build_cmd_send_flex(raw, dev->seq++, uuid, capcode, frequency,
                              power, mail_drop, message, msg_len);

    if (_flex_send_raw(dev, raw) < 0) return -1;

    // Wait for ACK
    flex_packet_t rsp;
    if (_flex_recv_packet(dev, &rsp, 5000) < 0) {
        fprintf(stderr, "flex: timeout waiting for ACK\n");
        return -1;
    }

    if (!(rsp.pkt_type == FLEX_PKT_RSP && rsp.opcode == FLEX_RSP_ACK)) {
        if (rsp.pkt_type == FLEX_PKT_RSP && rsp.opcode == FLEX_RSP_NACK)
            fprintf(stderr, "flex: NACK error=0x%02X\n", rsp.payload[0]);
        else
            fprintf(stderr, "flex: unexpected response type=0x%02X opcode=0x%02X\n",
                    rsp.pkt_type, rsp.opcode);
        return -1;
    }

    if (rsp.payload[0] != FLEX_STATUS_ACCEPTED) {
        fprintf(stderr, "flex: ACK rejected status=0x%02X\n", rsp.payload[0]);
        return -1;
    }

    printf("ACK: Message accepted\n");

    // Wait for TX_DONE/TX_FAILED events
    time_t deadline = time(NULL) + wait_timeout_sec;
    while (time(NULL) < deadline) {
        flex_packet_t evt;
        if (_flex_recv_packet(dev, &evt, 1000) < 0) continue;
        if (evt.pkt_type != FLEX_PKT_EVT) continue;

        char evt_uuid_str[37];
        _flex_uuid_to_str(evt.uuid, evt_uuid_str);
        bool uuid_match = (memcmp(evt.uuid, uuid, 16) == 0);

        switch (evt.opcode) {
            case FLEX_EVT_TX_QUEUED:
                if (dev->verbose)
                    printf("EVENT: TX_QUEUED pos=%d (uuid=%s)\n",
                           evt.payload[0], evt_uuid_str);
                break;

            case FLEX_EVT_TX_START:
                printf("EVENT: TX_START (uuid=%s)\n", evt_uuid_str);
                break;

            case FLEX_EVT_TX_DONE:
                if (uuid_match) {
                    if (evt.payload[0] == FLEX_RESULT_SUCCESS) {
                        printf("EVENT: TX_DONE SUCCESS (uuid=%s)\n", evt_uuid_str);
                        return 0;
                    }
                    fprintf(stderr, "EVENT: TX_DONE FAILED result=0x%02X (uuid=%s)\n",
                            evt.payload[0], evt_uuid_str);
                    return -1;
                } else if (dev->verbose) {
                    printf("EVENT: TX_DONE for other message (uuid=%s)\n", evt_uuid_str);
                }
                break;

            case FLEX_EVT_TX_FAILED:
                if (uuid_match) {
                    fprintf(stderr, "EVENT: TX_FAILED result=0x%02X (uuid=%s)\n",
                            evt.payload[0], evt_uuid_str);
                    return -1;
                } else if (dev->verbose) {
                    printf("EVENT: TX_FAILED for other message (uuid=%s)\n", evt_uuid_str);
                }
                break;

            default:
                if (dev->verbose)
                    printf("EVENT: opcode=0x%02X (uuid=%s)\n", evt.opcode, evt_uuid_str);
                break;
        }
    }

    fprintf(stderr, "flex: timeout waiting for TX_DONE\n");
    return -1;
}

/*
 * flex_ping - Send PING and wait for PONG.
 *
 * Returns 0 on success, -1 on timeout/error.
 */
static inline int flex_ping(FlexDevice *dev) {
    uint8_t uuid[16];
    _flex_uuid_v4(uuid);

    uint8_t raw[FLEX_PACKET_SIZE];
    _flex_build_cmd_ping(raw, dev->seq++, uuid);

    if (_flex_send_raw(dev, raw) < 0) return -1;

    flex_packet_t rsp;
    if (_flex_recv_packet(dev, &rsp, 3000) < 0) {
        fprintf(stderr, "flex: ping timeout\n");
        return -1;
    }

    if (rsp.pkt_type == FLEX_PKT_RSP && rsp.opcode == FLEX_RSP_PONG) return 0;

    fprintf(stderr, "flex: unexpected ping response type=0x%02X opcode=0x%02X\n",
            rsp.pkt_type, rsp.opcode);
    return -1;
}

/*
 * flex_get_status - Query device status.
 * Parsed fields returned via output parameters (all may be NULL).
 *
 * Returns 0 on success, -1 on error.
 */
static inline int flex_get_status(FlexDevice *dev, uint8_t *device_state,
                                   uint8_t *queue_count, uint8_t *battery_pct,
                                   uint16_t *battery_mv, float *frequency, int8_t *power) {
    uint8_t uuid[16];
    _flex_uuid_v4(uuid);

    uint8_t raw[FLEX_PACKET_SIZE];
    _flex_build_cmd_get_status(raw, dev->seq++, uuid);

    if (_flex_send_raw(dev, raw) < 0) return -1;

    flex_packet_t rsp;
    if (_flex_recv_packet(dev, &rsp, 3000) < 0) {
        fprintf(stderr, "flex: get_status timeout\n");
        return -1;
    }

    if (!(rsp.pkt_type == FLEX_PKT_RSP && rsp.opcode == FLEX_RSP_STATUS)) {
        fprintf(stderr, "flex: unexpected status response type=0x%02X opcode=0x%02X\n",
                rsp.pkt_type, rsp.opcode);
        return -1;
    }

    // RSP_STATUS payload layout (from binary_packet.h rsp_status_payload_t):
    // [0]   device_state (uint8)
    // [1]   queue_count  (uint8)
    // [2]   battery_pct  (uint8)
    // [3-4] battery_mv   (uint16, little-endian from ESP32)
    // [5-8] frequency    (float, little-endian)
    // [9]   power        (int8)
    if (device_state) *device_state = rsp.payload[0];
    if (queue_count)  *queue_count  = rsp.payload[1];
    if (battery_pct)  *battery_pct  = rsp.payload[2];
    if (battery_mv)   memcpy(battery_mv, rsp.payload + 3, 2);
    if (frequency)    memcpy(frequency,  rsp.payload + 5, 4);
    if (power)        *power = (int8_t)rsp.payload[9];

    return 0;
}

#endif // FLEXDEVICE_H
