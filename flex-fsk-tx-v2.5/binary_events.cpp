/*
 * FLEX Paging Message Transmitter - v2.5.2
 * Binary Protocol - Event Senders Implementation
 */

#include "binary_events.h"
#include "binary_packet.h"
#include "uuid.h"
#include "cobs.h"
#include "crc16.h"
#include "logging.h"

// Serial mutex (extern from logging.h)
extern SemaphoreHandle_t serial_mutex;

bool binary_protocol_active = false;

static void send_packet_via_serial(const binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    uint8_t cobs_buffer[PACKET_FIXED_SIZE + 10];
    size_t cobs_len = cobs_encode((uint8_t*)pkt, PACKET_FIXED_SIZE, cobs_buffer);

    if (cobs_len == 0) {
        logMessage("BINARY: COBS encode failed");
        return;
    }

#ifdef ENABLE_DEBUG
    if (pkt->type == PKT_TYPE_EVT && (pkt->opcode == EVT_TX_DONE || pkt->opcode == EVT_TX_START)) {
        char head_hex[64];
        size_t preview_len = (cobs_len < 8) ? cobs_len : 8;
        char* cursor = head_hex;
        for (size_t i = 0; i < preview_len; ++i) {
            int written = snprintf(cursor, head_hex + sizeof(head_hex) - cursor, "%02X", cobs_buffer[i]);
            if (written <= 0 || (size_t)written >= (size_t)(head_hex + sizeof(head_hex) - cursor)) {
                break;
            }
            cursor += written;
            if (i + 1 < preview_len) {
                *cursor++ = ' ';
            }
        }
        *cursor = '\0';
        logMessagef("BINARY: EVT opcode=0x%02X COBS head[%u]=%s len=%u",
                    pkt->opcode, (unsigned)preview_len, head_hex, (unsigned)cobs_len);
    }

    if (cobs_len < PACKET_FIXED_SIZE) {
        logMessagef("BINARY: COBS truncated len=%u type=0x%02X opcode=0x%02X",
                    (unsigned)cobs_len, pkt->type, pkt->opcode);
    }
#endif

    if (xSemaphoreTake(serial_mutex, portMAX_DELAY) == pdTRUE) {
        Serial.flush();
        size_t written = Serial.write(cobs_buffer, cobs_len);
#ifdef ENABLE_DEBUG
        if (written != cobs_len) {
            logMessagef("BINARY: Serial write short (expected=%u wrote=%u type=0x%02X)",
                        (unsigned)cobs_len, (unsigned)written, pkt->type);
        }
#endif
        Serial.flush();
        xSemaphoreGive(serial_mutex);
    }
}

void send_evt_tx_queued(const uint8_t uuid[16], uint8_t pos) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_queued(&pkt, uuid, pos);
    send_packet_via_serial(&pkt);
}

void send_evt_tx_start(const uint8_t uuid[16]) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_start(&pkt, uuid);
    send_packet_via_serial(&pkt);
}

void send_evt_tx_done(const uint8_t uuid[16], uint8_t result) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_done(&pkt, uuid, result);
    send_packet_via_serial(&pkt);
}

void send_evt_tx_failed(const uint8_t uuid[16], uint8_t error) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_failed(&pkt, uuid, error);
    send_packet_via_serial(&pkt);
}

void send_evt_boot() {
    if (!binary_protocol_active) {
        return;
    }

    binary_packet_t pkt;
    memset(&pkt, 0, PACKET_FIXED_SIZE);

    pkt.type = PKT_TYPE_EVT;
    pkt.opcode = EVT_BOOT;
    pkt.flags = 0x00;
    memset(pkt.uuid, 0, 16);
    pkt.payload_len = 0;

    uint16_t crc = crc16_ccitt((uint8_t*)&pkt, PACKET_CRC_OFFSET);
    pkt.crc16 = crc;

    send_packet_via_serial(&pkt);
}

void send_evt_battery_low(uint8_t battery_pct) {
    if (!binary_protocol_active) {
        return;
    }

    binary_packet_t pkt;
    memset(&pkt, 0, PACKET_FIXED_SIZE);

    pkt.type = PKT_TYPE_EVT;
    pkt.opcode = EVT_BATTERY_LOW;
    pkt.flags = 0x00;
    memset(pkt.uuid, 0, 16);
    pkt.payload_len = htons_custom(1);
    pkt.payload[0] = battery_pct;

    uint16_t crc = crc16_ccitt((uint8_t*)&pkt, PACKET_CRC_OFFSET);
    pkt.crc16 = crc;

    send_packet_via_serial(&pkt);
}

void send_evt_power_disconnected() {
    if (!binary_protocol_active) {
        return;
    }

    binary_packet_t pkt;
    memset(&pkt, 0, PACKET_FIXED_SIZE);

    pkt.type = PKT_TYPE_EVT;
    pkt.opcode = EVT_POWER_DISCONNECTED;
    pkt.flags = 0x00;
    memset(pkt.uuid, 0, 16);
    pkt.payload_len = 0;

    uint16_t crc = crc16_ccitt((uint8_t*)&pkt, PACKET_CRC_OFFSET);
    pkt.crc16 = crc;

    send_packet_via_serial(&pkt);
}

void send_binary_response_ack(const uint8_t uuid[16], uint8_t status) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_rsp_ack(&pkt, uuid, status);
    send_packet_via_serial(&pkt);
}

void send_binary_response_nack(const uint8_t uuid[16], uint8_t status) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_rsp_nack(&pkt, uuid, status);
    send_packet_via_serial(&pkt);
}

void send_binary_response_pong(const uint8_t uuid[16]) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_rsp_pong(&pkt, uuid);
    send_packet_via_serial(&pkt);
}

void send_binary_response_status(const uint8_t uuid[16],
                                 uint8_t device_state, uint8_t queue_count,
                                 uint8_t battery_pct, uint16_t battery_mv,
                                 float frequency, int8_t power) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    rsp_status_payload_t status;
    status.device_state = device_state;
    status.queue_count = queue_count;
    status.battery_pct = battery_pct;
    status.battery_mv = battery_mv;
    status.frequency = frequency;
    status.power = power;

    binary_packet_t pkt;
    build_rsp_status(&pkt, uuid, &status);
    send_packet_via_serial(&pkt);
}
