/*
 * FLEX Paging Message Transmitter - v2.5.2
 * UUID Generation and Utilities
 *
 * RFC 4122 UUID v4 implementation for message tracking
 */

#ifndef UUID_H
#define UUID_H

#include <Arduino.h>
#include <stdint.h>

// =============================================================================
// UUID GENERATION
// =============================================================================

/**
 * Generate a random UUID v4 (RFC 4122)
 *
 * Uses ESP32 hardware RNG (esp_random) for cryptographically secure randomness
 *
 * @param uuid  Output buffer for 16-byte UUID
 */
void generate_uuid_v4(uint8_t uuid[16]);

// =============================================================================
// UUID STRING CONVERSION
// =============================================================================

/**
 * Convert UUID bytes to string format
 *
 * Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 * Example: b0273db2-53e5-430c-9d96-40a8d388c57a
 *
 * @param uuid  Input 16-byte UUID
 * @param str   Output buffer (minimum 37 bytes for null terminator)
 */
void uuid_to_string(const uint8_t uuid[16], char *str);

/**
 * Parse UUID string to bytes
 *
 * @param str   Input string in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 * @param uuid  Output buffer for 16-byte UUID
 * @return      true if parsing successful, false otherwise
 */
bool uuid_from_string(const char *str, uint8_t uuid[16]);

/**
 * Compare two UUIDs for equality
 *
 * @param uuid1  First UUID
 * @param uuid2  Second UUID
 * @return       true if equal, false otherwise
 */
bool uuid_equals(const uint8_t uuid1[16], const uint8_t uuid2[16]);

/**
 * Copy UUID
 *
 * @param dest  Destination buffer
 * @param src   Source UUID
 */
void uuid_copy(uint8_t dest[16], const uint8_t src[16]);

#endif // UUID_H
