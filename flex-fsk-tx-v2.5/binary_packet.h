/*
 * FLEX Paging Message Transmitter - v2.5.4
 * Binary Protocol - Packet Structure
 *
 * Defines packet structure, opcodes, and packet building/parsing functions
 *
 * v2.5.4 Changelog:
 * - Added AT+CCLK command for manual clock synchronization
 *
 * v2.5.3 Changelog:
 * - Added timestamp header (8 bytes) in binary packets
 * - Reduced payload from 486 to 478 bytes
 * - Timestamp positioned at bytes 502-509 (before CRC)
 * - CRC remains at bytes 510-511 (unchanged)
 */

#ifndef BINARY_PACKET_H
#define BINARY_PACKET_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include "config.h"

// =============================================================================
// TIMESTAMP HEADER STRUCTURE (8 bytes)
// =============================================================================

typedef struct __attribute__((packed)) {
    uint32_t unix_timestamp;    // [0-3]  UTC seconds since 1970 (big-endian)
    uint16_t milliseconds;      // [4-5]  Subseconds (0-999) (big-endian)
    int8_t   timezone_offset;   // [6]    Timezone in 30-min units (-48 to +56)
    uint8_t  flags;             // [7]    Control flags
} timestamp_header_t;

// Timestamp flags
#define TS_FLAG_VALID       0x01  // Timestamp is valid
#define TS_FLAG_AUTO_ADJUST 0x02  // Auto-adjust device clock if drift > 1s
#define TS_FLAG_SYNC_RTC    0x04  // Sync RTC hardware
#define TS_FLAG_DST_ACTIVE  0x08  // Daylight Saving Time active

// =============================================================================
// PACKET CONSTANTS
// =============================================================================

// Fixed packet size - ALL packets are EXACTLY 512 bytes
#define PACKET_FIXED_SIZE 512
#define PACKET_HEADER_SIZE 22            // type + opcode + flags + seq + uuid + payload_len
#define PACKET_PAYLOAD_SIZE 480          // Fixed payload area
#define PACKET_TIMESTAMP_SIZE 8          // Timestamp header size
#define PACKET_CRC_SIZE 2                // CRC16 field size
#define PACKET_CRC_OFFSET 510            // CRC always at offset 510-511

// Protocol limits
#define MAX_MESSAGE_IN_PROTOCOL 255      // msg_len is uint8_t (1 byte)
#define CMD_SEND_FLEX_ARGS_SIZE 11       // capcode(4) + freq(4) + power(1) + mail(1) + len(1)

// =============================================================================
// PACKET TYPES
// =============================================================================

#define PKT_TYPE_CMD 0x01   // Command (Host → ESP32)
#define PKT_TYPE_RSP 0x02   // Response (ESP32 → Host, immediate)
#define PKT_TYPE_EVT 0x03   // Event (ESP32 → Host, async)

// =============================================================================
// OPCODES: COMMANDS (TYPE=CMD)
// =============================================================================

#define CMD_SEND_FLEX     0x01  // Send FLEX message
#define CMD_GET_STATUS    0x02  // Query device status
#define CMD_ABORT         0x03  // Abort current operation
#define CMD_SET_CONFIG    0x04  // Set configuration
#define CMD_GET_CONFIG    0x05  // Get configuration
#define CMD_PING          0x06  // Heartbeat/connectivity test
#define CMD_GET_LOGS      0x07  // Query persistent logs
#define CMD_CLEAR_LOGS    0x08  // Delete log file
#define CMD_FACTORY_RESET 0x09  // Factory reset device

// =============================================================================
// OPCODES: RESPONSES (TYPE=RSP)
// =============================================================================

#define RSP_ACK     0x01  // Command accepted
#define RSP_NACK    0x02  // Command rejected
#define RSP_STATUS  0x03  // Status response
#define RSP_CONFIG  0x04  // Config response
#define RSP_PONG    0x05  // Ping response
#define RSP_LOGS    0x06  // Log data response

// =============================================================================
// OPCODES: EVENTS (TYPE=EVT)
// =============================================================================

#define EVT_TX_QUEUED          0x01  // Message enqueued
#define EVT_TX_START           0x02  // Transmission started
#define EVT_TX_DONE            0x03  // Transmission completed
#define EVT_TX_FAILED          0x04  // Transmission failed
#define EVT_BOOT               0x05  // Device boot notification
#define EVT_ERROR              0x06  // Error notification
#define EVT_BATTERY_LOW        0x07  // Low battery alert
#define EVT_POWER_DISCONNECTED 0x08  // Power disconnect alert

// =============================================================================
// FLAGS (Control Bits)
// =============================================================================

#define FLAG_ACK_REQUIRED  (1 << 0)  // 0x01 - ACK required
#define FLAG_RETRY         (1 << 1)  // 0x02 - Retry of previous packet
#define FLAG_ERROR         (1 << 2)  // 0x04 - Error flag
#define FLAG_PRIORITY      (1 << 3)  // 0x08 - High priority
#define FLAG_FRAGMENTED    (1 << 4)  // 0x10 - Fragmented packet
#define FLAG_LAST_FRAGMENT (1 << 5)  // 0x20 - Last fragment

// =============================================================================
// STATUS CODES (RSP_ACK/RSP_NACK payload)
// =============================================================================

#define STATUS_ACCEPTED      0x00  // Command accepted
#define STATUS_REJECTED      0x01  // Command rejected
#define STATUS_QUEUE_FULL    0x02  // Message queue full
#define STATUS_INVALID_PARAM 0x03  // Invalid parameter
#define STATUS_BUSY          0x04  // Device busy
#define STATUS_ERROR         0x05  // Generic error

// =============================================================================
// RESULT CODES (EVT_TX_DONE/EVT_TX_FAILED payload)
// =============================================================================

#define RESULT_SUCCESS         0x00  // Transmission successful
#define RESULT_RADIO_ERROR     0x01  // Radio hardware error
#define RESULT_ENCODING_ERROR  0x02  // FLEX encoding error
#define RESULT_TIMEOUT         0x03  // Timeout
#define RESULT_ABORTED         0x04  // Aborted by user

// =============================================================================
// BINARY PACKET STRUCTURE - FIXED 512 BYTES
// =============================================================================

/**
 * Binary packet structure - ALWAYS 512 bytes
 *
 * Layout:
 * [0]      type           - Packet type (CMD/RSP/EVT)
 * [1]      opcode         - Operation code
 * [2]      flags          - Control flags
 * [3]      seq            - Sequence number
 * [4-19]   uuid           - 128-bit UUID for message tracking
 * [20-21]  payload_len    - Valid bytes in payload (big-endian)
 * [22-501] payload        - Fixed 480-byte payload area
 * [502-509] ts            - Timestamp header (8 bytes) - AFTER payload
 * [510-511] crc16         - CRC16-CCITT (ALWAYS at this offset)
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  opcode;
    uint8_t  flags;
    uint8_t  seq;
    uint8_t  uuid[16];
    uint16_t payload_len;
    uint8_t  payload[PACKET_PAYLOAD_SIZE];
    timestamp_header_t ts;
    uint16_t crc16;
} binary_packet_t;

// Compile-time size verification
static_assert(sizeof(binary_packet_t) == PACKET_FIXED_SIZE,
              "binary_packet_t must be exactly 512 bytes");

// =============================================================================
// PAYLOAD STRUCTURES
// =============================================================================

// CMD_SEND_FLEX payload layout (within packet.payload[480])
// [0-3]   capcode (4 bytes, little-endian)
// [4-7]   frequency (4 bytes, IEEE 754 float)
// [8]     tx_power (1 byte, signed)
// [9]     mail_drop (1 byte, 0=false 1=true)
// [10]    msg_len (1 byte, 1-255)
// [11-XX] message (0-255 bytes, actual length in msg_len)
//
// Note: Message can be up to 255 bytes in protocol,
//       but FLEX encoding limits to 248 bytes (firmware truncates if needed)

typedef struct __attribute__((packed)) {
    uint8_t status;
} rsp_ack_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t device_state;
    uint8_t queue_count;
    uint8_t battery_pct;
    uint16_t battery_mv;
    float frequency;
    int8_t power;
} rsp_status_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t queue_pos;
} evt_tx_queued_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t result;
} evt_tx_done_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t error_code;
} evt_tx_failed_payload_t;

// =============================================================================
// FUNCTION PROTOTYPES: PACKET BUILDING
// =============================================================================

/**
 * Build CMD_SEND_FLEX packet
 *
 * @param pkt       Output packet buffer
 * @param seq       Transport sequence number
 * @param msg_id    Application message ID
 * @param capcode   FLEX capcode
 * @param frequency Frequency in MHz
 * @param tx_power  TX power in dBm
 * @param mail_drop Mail drop flag (0=false, 1=true)
 * @param message   Message text (null-terminated)
 * @param msg_len   Message length (1-248)
 * @return          Total packet length (including CRC)
 */
size_t build_cmd_send_flex(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16],
                           uint32_t capcode, float frequency, int8_t tx_power,
                           uint8_t mail_drop, const char *message, uint8_t msg_len);

/**
 * Build RSP_ACK packet
 *
 * @param pkt       Output packet buffer
 * @param seq       Transport sequence number (from received command)
 * @param msg_id    Application message ID (from received command)
 * @param status    Status code (STATUS_ACCEPTED, etc.)
 * @return          Total packet length
 */
size_t build_rsp_ack(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t status);

/**
 * Build RSP_NACK packet (same as RSP_ACK but different opcode)
 *
 * @param pkt       Output packet buffer
 * @param seq       Transport sequence number
 * @param msg_id    Application message ID
 * @param status    Status code (STATUS_REJECTED, etc.)
 * @return          Total packet length
 */
size_t build_rsp_nack(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t status);

/**
 * Build RSP_STATUS packet
 *
 * @param pkt       Output packet buffer
 * @param seq       Transport sequence number
 * @param msg_id    Application message ID
 * @param status    Status payload
 * @return          Total packet length
 */
size_t build_rsp_status(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16],
                        const rsp_status_payload_t *status);

/**
 * Build RSP_PONG packet
 *
 * @param pkt       Output packet buffer
 * @param seq       Transport sequence number
 * @param msg_id    Application message ID
 * @return          Total packet length
 */
size_t build_rsp_pong(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16]);

/**
 * Build EVT_TX_QUEUED packet
 *
 * @param pkt       Output packet buffer
 * @param seq       Transport sequence number
 * @param msg_id    Application message ID
 * @param pos       Queue position (1-10)
 * @return          Total packet length
 */
size_t build_evt_tx_queued(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t pos);

/**
 * Build EVT_TX_START packet
 */
size_t build_evt_tx_start(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16]);

/**
 * Build EVT_TX_DONE packet
 */
size_t build_evt_tx_done(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t result);

/**
 * Build EVT_TX_FAILED packet
 */
size_t build_evt_tx_failed(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t error);

// =============================================================================
// FUNCTION PROTOTYPES: PACKET PARSING
// =============================================================================

/**
 * Parse binary packet from raw data
 *
 * @param data      Raw packet data (already COBS-decoded)
 * @param len       Length of data
 * @param pkt       Output packet structure
 * @return          true if parse successful, false otherwise
 *
 * This function does NOT validate CRC. Use validate_packet() separately.
 */
bool parse_packet(const uint8_t *data, size_t len, binary_packet_t *pkt);

/**
 * Validate packet (check CRC)
 *
 * @param pkt       Packet to validate
 * @return          true if CRC is valid, false otherwise
 */
bool validate_packet(const binary_packet_t *pkt);

// =============================================================================
// HELPER MACROS
// =============================================================================

#define ntohs_custom(x) ((uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))
#define htons_custom(x) ntohs_custom(x)
#define ntohl_custom(x) ((uint32_t)(((x) & 0xFF000000) >> 24) | \
                                    (((x) & 0x00FF0000) >> 8)  | \
                                    (((x) & 0x0000FF00) << 8)  | \
                                    (((x) & 0x000000FF) << 24))
#define htonl_custom(x) ntohl_custom(x)
#define PACKET_PAYLOAD(pkt) ((pkt)->payload)

#endif // BINARY_PACKET_H
