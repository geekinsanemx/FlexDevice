/*
 * FLEX Paging Message Transmitter - v2.5.4
 * Binary Protocol - Packet Implementation
 *
 * v2.5.4: Added AT+CCLK command support
 * v2.5.3: Added timestamp population in all build_* functions
 */

#include "binary_packet.h"
#include "uuid.h"
#include "crc16.h"
#include <string.h>
#include <sys/time.h>

// External variables from hardware.cpp
extern bool system_time_initialized;
extern float timezone_offset_hours;

// =============================================================================
// TIMESTAMP POPULATION
// =============================================================================

static void populate_packet_timestamp(binary_packet_t *pkt) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Populate timestamp (big-endian for network byte order)
    pkt->ts.unix_timestamp = htonl_custom((uint32_t)tv.tv_sec);
    pkt->ts.milliseconds = htons_custom((uint16_t)(tv.tv_usec / 1000));

    // Timezone in 30-minute increments
    int8_t tz_units = (int8_t)(timezone_offset_hours * 2.0f);
    pkt->ts.timezone_offset = tz_units;

    // Flags: valid if system time initialized
    pkt->ts.flags = system_time_initialized ? TS_FLAG_VALID : 0x00;
}

// =============================================================================
// PACKET BUILDING: COMMANDS
// =============================================================================

size_t build_cmd_send_flex(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16],
                           uint64_t capcode, float frequency, int8_t tx_power,
                           uint8_t mail_drop, const char *message, uint8_t msg_len) {
    if (pkt == nullptr || uuid == nullptr || message == nullptr) {
        return 0;
    }

    if (msg_len > MAX_MESSAGE_IN_PROTOCOL) {
        msg_len = MAX_MESSAGE_IN_PROTOCOL;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_CMD;
    pkt->opcode = CMD_SEND_FLEX;
    pkt->flags = FLAG_ACK_REQUIRED;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(CMD_SEND_FLEX_ARGS_SIZE + msg_len);

    memcpy(&pkt->payload[0], &capcode, 8);
    memcpy(&pkt->payload[8], &frequency, 4);
    pkt->payload[12] = (uint8_t)tx_power;
    pkt->payload[13] = mail_drop;
    pkt->payload[14] = msg_len;
    memcpy(&pkt->payload[15], message, msg_len);

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

// =============================================================================
// PACKET BUILDING: RESPONSES
// =============================================================================

size_t build_rsp_ack(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t status) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_RSP;
    pkt->opcode = RSP_ACK;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(1);

    pkt->payload[0] = status;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

size_t build_rsp_nack(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t status) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_RSP;
    pkt->opcode = RSP_NACK;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(1);

    pkt->payload[0] = status;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

size_t build_rsp_status(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16],
                        const rsp_status_payload_t *status) {
    if (pkt == nullptr || uuid == nullptr || status == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_RSP;
    pkt->opcode = RSP_STATUS;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(sizeof(rsp_status_payload_t));

    memcpy(pkt->payload, status, sizeof(rsp_status_payload_t));

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

size_t build_rsp_pong(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16]) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_RSP;
    pkt->opcode = RSP_PONG;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = 0;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

// =============================================================================
// PACKET BUILDING: EVENTS
// =============================================================================

size_t build_evt_tx_queued(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t pos) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_EVT;
    pkt->opcode = EVT_TX_QUEUED;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(1);

    pkt->payload[0] = pos;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

size_t build_evt_tx_start(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16]) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_EVT;
    pkt->opcode = EVT_TX_START;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = 0;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

size_t build_evt_tx_done(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t result) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_EVT;
    pkt->opcode = EVT_TX_DONE;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(1);

    pkt->payload[0] = result;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

size_t build_evt_tx_failed(binary_packet_t *pkt, uint8_t seq, const uint8_t uuid[16], uint8_t error) {
    if (pkt == nullptr || uuid == nullptr) {
        return 0;
    }

    memset(pkt, 0, PACKET_FIXED_SIZE);

    pkt->type = PKT_TYPE_EVT;
    pkt->opcode = EVT_TX_FAILED;
    pkt->flags = 0x00;
    pkt->seq = seq;
    uuid_copy(pkt->uuid, uuid);
    pkt->payload_len = htons_custom(1);

    pkt->payload[0] = error;

    // Populate timestamp
    populate_packet_timestamp(pkt);

    uint16_t crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    pkt->crc16 = crc;

    return PACKET_FIXED_SIZE;
}

// =============================================================================
// PACKET PARSING AND VALIDATION
// =============================================================================

bool parse_packet(const uint8_t *data, size_t len, binary_packet_t *pkt) {
    if (data == nullptr || pkt == nullptr) {
        return false;
    }

    if (len != PACKET_FIXED_SIZE) {
        return false;
    }

    memcpy(pkt, data, PACKET_FIXED_SIZE);
    return true;
}

bool validate_packet(const binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return false;
    }

    uint16_t calculated_crc = crc16_ccitt((uint8_t*)pkt, PACKET_CRC_OFFSET);
    return (calculated_crc == pkt->crc16);
}
