/*
 * FLEX Paging Message Transmitter - v2.5.2
 * Binary Protocol - Event Senders
 */

#ifndef BINARY_EVENTS_H
#define BINARY_EVENTS_H

#include <Arduino.h>
#include <stdint.h>

extern bool binary_protocol_active;
extern uint8_t binary_event_seq;

/**
 * Send EVT_TX_QUEUED event
 */
void send_evt_tx_queued(const uint8_t uuid[16], uint8_t pos);

/**
 * Send EVT_TX_START event
 */
void send_evt_tx_start(const uint8_t uuid[16]);

/**
 * Send EVT_TX_DONE event
 */
void send_evt_tx_done(const uint8_t uuid[16], uint8_t result);

/**
 * Send EVT_TX_FAILED event
 */
void send_evt_tx_failed(const uint8_t uuid[16], uint8_t error);

/**
 * Send EVT_BOOT event
 */
void send_evt_boot();

/**
 * Send EVT_BATTERY_LOW event
 */
void send_evt_battery_low(uint8_t battery_pct);

/**
 * Send EVT_POWER_DISCONNECTED event
 */
void send_evt_power_disconnected();

/**
 * Send RSP_ACK response
 */
void send_binary_response_ack(uint8_t seq, const uint8_t uuid[16], uint8_t status);

/**
 * Send RSP_NACK response
 */
void send_binary_response_nack(uint8_t seq, const uint8_t uuid[16], uint8_t status);

/**
 * Send RSP_PONG response
 */
void send_binary_response_pong(uint8_t seq, const uint8_t uuid[16]);

/**
 * Send RSP_STATUS response
 */
void send_binary_response_status(uint8_t seq, const uint8_t uuid[16],
                                 uint8_t device_state, uint8_t queue_count,
                                 uint8_t battery_pct, uint16_t battery_mv,
                                 float frequency, int8_t power);

#endif
