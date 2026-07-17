/*
 * FLEX Paging Message Transmitter - v2.5.2
 * UUID Generation and Utilities Implementation
 */

#include "uuid.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// UUID GENERATION
// =============================================================================

void generate_uuid_v4(uint8_t uuid[16]) {
    if (uuid == nullptr) {
        return;
    }

    // Generate 128 bits of random data using ESP32 hardware RNG
    esp_fill_random(uuid, 16);

    // Set version to 4 (random UUID) - RFC 4122 section 4.1.3
    // Bits 12-15 of time_hi_and_version field = 0100
    uuid[6] = (uuid[6] & 0x0F) | 0x40;

    // Set variant to DCE 1.1, ISO/IEC 11578:1996 - RFC 4122 section 4.1.1
    // Bits 6-7 of clock_seq_hi_and_reserved field = 10
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
}

// =============================================================================
// UUID STRING CONVERSION
// =============================================================================

void uuid_to_string(const uint8_t uuid[16], char *str) {
    if (uuid == nullptr || str == nullptr) {
        return;
    }

    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3],
            uuid[4], uuid[5], uuid[6], uuid[7],
            uuid[8], uuid[9], uuid[10], uuid[11],
            uuid[12], uuid[13], uuid[14], uuid[15]);
}

bool uuid_from_string(const char *str, uint8_t uuid[16]) {
    if (str == nullptr || uuid == nullptr) {
        return false;
    }

    // Expected format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars)
    if (strlen(str) != 36) {
        return false;
    }

    // Check hyphens at correct positions
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
        return false;
    }

    // Parse hex values
    int matched = sscanf(str,
                         "%02hhx%02hhx%02hhx%02hhx-"
                         "%02hhx%02hhx-"
                         "%02hhx%02hhx-"
                         "%02hhx%02hhx-"
                         "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
                         &uuid[0], &uuid[1], &uuid[2], &uuid[3],
                         &uuid[4], &uuid[5], &uuid[6], &uuid[7],
                         &uuid[8], &uuid[9], &uuid[10], &uuid[11],
                         &uuid[12], &uuid[13], &uuid[14], &uuid[15]);

    return (matched == 16);
}

// =============================================================================
// UUID UTILITIES
// =============================================================================

bool uuid_equals(const uint8_t uuid1[16], const uint8_t uuid2[16]) {
    if (uuid1 == nullptr || uuid2 == nullptr) {
        return false;
    }

    return (memcmp(uuid1, uuid2, 16) == 0);
}

void uuid_copy(uint8_t dest[16], const uint8_t src[16]) {
    if (dest == nullptr || src == nullptr) {
        return;
    }

    memcpy(dest, src, 16);
}
