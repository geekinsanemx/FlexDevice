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

bool binary_protocol_active = false;
uint8_t binary_event_seq = 0;

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

    Serial.write(cobs_buffer, cobs_len);
    Serial.flush();
}

void send_evt_tx_queued(const uint8_t uuid[16], uint8_t pos) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_queued(&pkt, binary_event_seq++, uuid, pos);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: EVT_TX_QUEUED uuid=%s pos=%d", uuid_str, pos);
}

void send_evt_tx_start(const uint8_t uuid[16]) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_start(&pkt, binary_event_seq++, uuid);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: EVT_TX_START uuid=%s", uuid_str);
}

void send_evt_tx_done(const uint8_t uuid[16], uint8_t result) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_done(&pkt, binary_event_seq++, uuid, result);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: EVT_TX_DONE uuid=%s result=%d", uuid_str, result);
}

void send_evt_tx_failed(const uint8_t uuid[16], uint8_t error) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_evt_tx_failed(&pkt, binary_event_seq++, uuid, error);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: EVT_TX_FAILED uuid=%s error=%d", uuid_str, error);
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
    pkt.seq = binary_event_seq++;
    memset(pkt.uuid, 0, 16);
    pkt.payload_len = 0;

    uint16_t crc = crc16_ccitt((uint8_t*)&pkt, PACKET_CRC_OFFSET);
    pkt.crc16 = crc;

    send_packet_via_serial(&pkt);
    logMessage("BINARY: EVT_BOOT sent");
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
    pkt.seq = binary_event_seq++;
    memset(pkt.uuid, 0, 16);
    pkt.payload_len = htons_custom(1);
    pkt.payload[0] = battery_pct;

    uint16_t crc = crc16_ccitt((uint8_t*)&pkt, PACKET_CRC_OFFSET);
    pkt.crc16 = crc;

    send_packet_via_serial(&pkt);
    logMessagef("BINARY: EVT_BATTERY_LOW pct=%d", battery_pct);
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
    pkt.seq = binary_event_seq++;
    memset(pkt.uuid, 0, 16);
    pkt.payload_len = 0;

    uint16_t crc = crc16_ccitt((uint8_t*)&pkt, PACKET_CRC_OFFSET);
    pkt.crc16 = crc;

    send_packet_via_serial(&pkt);
    logMessage("BINARY: EVT_POWER_DISCONNECTED sent");
}

void send_binary_response_ack(uint8_t seq, const uint8_t uuid[16], uint8_t status) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_rsp_ack(&pkt, seq, uuid, status);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: RSP_ACK seq=%d uuid=%s status=%d", seq, uuid_str, status);
}

void send_binary_response_nack(uint8_t seq, const uint8_t uuid[16], uint8_t status) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_rsp_nack(&pkt, seq, uuid, status);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: RSP_NACK seq=%d uuid=%s status=%d", seq, uuid_str, status);
}

void send_binary_response_pong(uint8_t seq, const uint8_t uuid[16]) {
    if (!binary_protocol_active || uuid == nullptr) {
        return;
    }

    binary_packet_t pkt;
    build_rsp_pong(&pkt, seq, uuid);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: RSP_PONG seq=%d uuid=%s", seq, uuid_str);
}

void send_binary_response_status(uint8_t seq, const uint8_t uuid[16],
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
    build_rsp_status(&pkt, seq, uuid, &status);
    send_packet_via_serial(&pkt);

    char uuid_str[37];
    uuid_to_string(uuid, uuid_str);
    logMessagef("BINARY: RSP_STATUS seq=%d uuid=%s state=%d queue=%d",
                seq, uuid_str, device_state, queue_count);
}
