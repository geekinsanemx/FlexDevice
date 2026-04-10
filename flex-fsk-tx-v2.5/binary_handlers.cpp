/*
 * FLEX Paging Message Transmitter - v2.5.2
 * Binary Protocol - Command Handlers Implementation
 */

#include "binary_handlers.h"
#include "binary_events.h"
#include "binary_packet.h"
#include "uuid.h"
#include "flex_protocol.h"
#include "hardware.h"
#include "display.h"
#include "utils.h"
#include "logging.h"
#include "config.h"
#include "boards/boards.h"

extern volatile int queue_count;
extern float current_tx_frequency;
extern float current_tx_power;

void handle_cmd_send_flex(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    uint16_t payload_len = ntohs_custom(pkt->payload_len);
    if (payload_len < CMD_SEND_FLEX_ARGS_SIZE || payload_len > PACKET_PAYLOAD_SIZE) {
        char uuid_str[37];
        uuid_to_string(pkt->uuid, uuid_str);
        logMessagef("BINARY: Invalid payload_len=%d uuid=%s", payload_len, uuid_str);
        send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_INVALID_PARAM);
        return;
    }

    uint8_t *payload = pkt->payload;

    uint64_t capcode;
    memcpy(&capcode, &payload[0], 8);

    float frequency;
    memcpy(&frequency, &payload[8], 4);

    int8_t tx_power = (int8_t)payload[12];
    uint8_t mail_drop = payload[13];
    uint8_t msg_len = payload[14];

    if (msg_len < 1 || msg_len > MAX_MESSAGE_IN_PROTOCOL) {
        char uuid_str[37];
        uuid_to_string(pkt->uuid, uuid_str);
        logMessagef("BINARY: Invalid msg_len=%d (max=%d) uuid=%s",
                    msg_len, MAX_MESSAGE_IN_PROTOCOL, uuid_str);
        send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_INVALID_PARAM);
        return;
    }

    char safe_message[MAX_MESSAGE_IN_PROTOCOL + 1];
    memcpy(safe_message, &payload[15], msg_len);
    safe_message[msg_len] = '\0';

    if (msg_len > MAX_FLEX_MESSAGE_LENGTH) {
        safe_message[MAX_FLEX_MESSAGE_LENGTH - 3] = '.';
        safe_message[MAX_FLEX_MESSAGE_LENGTH - 2] = '.';
        safe_message[MAX_FLEX_MESSAGE_LENGTH - 1] = '.';
        safe_message[MAX_FLEX_MESSAGE_LENGTH] = '\0';

        char uuid_str[37];
        uuid_to_string(pkt->uuid, uuid_str);
        logMessagef("BINARY: Message truncated from %d to %d chars uuid=%s",
                    msg_len, MAX_FLEX_MESSAGE_LENGTH, uuid_str);
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_SEND_FLEX uuid=%s capcode=%llu freq=%.4f power=%d mail=%d len=%d",
                uuid_str, (unsigned long long)capcode, frequency, tx_power, mail_drop,
                (int)strlen(safe_message));

    if (!validate_flex_capcode(capcode)) {
        logMessage("BINARY: Invalid capcode");
        send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_INVALID_PARAM);
        return;
    }

    if (frequency < 400.0 || frequency > 1000.0) {
        logMessage("BINARY: Invalid frequency");
        send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_INVALID_PARAM);
        return;
    }

    if (tx_power < -1 || tx_power > 20) {
        logMessage("BINARY: Invalid TX power");
        send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_INVALID_PARAM);
        return;
    }

    bool mail_drop_flag = (mail_drop != 0);

    if (!queue_add_message_with_uuid(pkt->uuid, capcode, frequency,
                                      tx_power, mail_drop_flag, safe_message)) {
        logMessage("BINARY: Queue full");
        send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_QUEUE_FULL);
        return;
    }

    send_binary_response_ack(pkt->seq, pkt->uuid, STATUS_ACCEPTED);
    send_evt_tx_queued(pkt->uuid, queue_count);
}

void handle_cmd_get_status(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_GET_STATUS uuid=%s", uuid_str);

    uint8_t device_state = 0;
    uint16_t battery_voltage_mv;
    int battery_percentage;
    getBatteryInfo(&battery_voltage_mv, &battery_percentage);

    send_binary_response_status(pkt->seq, pkt->uuid, device_state, queue_count,
                                (uint8_t)battery_percentage, battery_voltage_mv,
                                current_tx_frequency, (int8_t)current_tx_power);
}

void handle_cmd_abort(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_ABORT uuid=%s", uuid_str);

    send_binary_response_ack(pkt->seq, pkt->uuid, STATUS_ACCEPTED);
}

void handle_cmd_set_config(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_SET_CONFIG uuid=%s", uuid_str);

    send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_REJECTED);
}

void handle_cmd_get_config(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_GET_CONFIG uuid=%s", uuid_str);

    send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_REJECTED);
}

void handle_cmd_ping(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_PING uuid=%s", uuid_str);

    send_binary_response_pong(pkt->seq, pkt->uuid);
}

void handle_cmd_get_logs(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_GET_LOGS uuid=%s", uuid_str);

    send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_REJECTED);
}

void handle_cmd_clear_logs(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_CLEAR_LOGS uuid=%s", uuid_str);

    send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_REJECTED);
}

void handle_cmd_factory_reset(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    char uuid_str[37];
    uuid_to_string(pkt->uuid, uuid_str);
    logMessagef("BINARY: CMD_FACTORY_RESET uuid=%s", uuid_str);

    send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_REJECTED);
}

void dispatch_binary_command(binary_packet_t *pkt) {
    if (pkt == nullptr) {
        return;
    }

    switch (pkt->opcode) {
        case CMD_SEND_FLEX:
            handle_cmd_send_flex(pkt);
            break;

        case CMD_GET_STATUS:
            handle_cmd_get_status(pkt);
            break;

        case CMD_ABORT:
            handle_cmd_abort(pkt);
            break;

        case CMD_SET_CONFIG:
            handle_cmd_set_config(pkt);
            break;

        case CMD_GET_CONFIG:
            handle_cmd_get_config(pkt);
            break;

        case CMD_PING:
            handle_cmd_ping(pkt);
            break;

        case CMD_GET_LOGS:
            handle_cmd_get_logs(pkt);
            break;

        case CMD_CLEAR_LOGS:
            handle_cmd_clear_logs(pkt);
            break;

        case CMD_FACTORY_RESET:
            handle_cmd_factory_reset(pkt);
            break;

        default:
            {
                char uuid_str[37];
                uuid_to_string(pkt->uuid, uuid_str);
                logMessagef("BINARY: Unknown command opcode 0x%02X uuid=%s",
                            pkt->opcode, uuid_str);
                send_binary_response_nack(pkt->seq, pkt->uuid, STATUS_REJECTED);
            }
            break;
    }
}
