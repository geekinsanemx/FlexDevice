/*
 * FLEX Paging Message Transmitter - UART/USB Slave Device
 * Version and Build Metadata
 *
 * FIRMWARE_VERSION is the single source of truth for the running version.
 * Every module that needs to display or report the version includes this
 * header instead of defining its own copy.
 */

#ifndef VERSION_H
#define VERSION_H

#define FIRMWARE_VERSION "v2.5.6"

#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// =============================================================================
// CHANGELOG
// =============================================================================
/*
 * v2.5.6 (2026-04-10):
 * - Fixed Serial TX buffer overflow causing packet truncation
 * - Increased TX buffer from 256 to 1024 bytes for COBS frames (514 bytes)
 * - Removed ASCII logs from binary event functions to prevent corruption
 * - Added Serial.flush() before binary packet writes
 * - Fixed -w flag: clients now properly receive TX_DONE events
 *
 * v2.5.5 (2026-04-09):
 * - Fixed capcode field size from 4 bytes to 8 bytes (uint32_t -> uint64_t)
 * - Binary protocol now supports full FLEX capcode range (up to 4,297,068,542)
 * - Updated CMD_SEND_FLEX payload offsets (frequency: 8-11, power: 12, etc.)
 * - Breaking change: clients must update to send 8-byte capcodes
 *
 * v2.5.4 (2026-04-05):
 * - Added AT+CCLK command for manual clock setting
 * - Format: AT+CCLK=<unix_timestamp>,<timezone_offset>
 * - Query: AT+CCLK? returns timestamp, timezone, and human-readable datetime
 * - Auto-syncs RTC if available
 * - Timezone sync from binary protocol packets
 * - Fixed segfault in client with invalid timestamps
 * - Latency measurement in verbose mode
 *
 * v2.5.3 (2026-04-05):
 * - Added timestamp header (8 bytes) in binary packets
 * - Reduced payload from 486 to 478 bytes for timestamp
 * - Client auto-includes system timestamp in packets
 * - ESP32 responds with its timestamp (latency measurement)
 * - Auto clock drift adjustment (> 1 sec) with RTC sync
 * - CRC remains at bytes 510-511 (unchanged)
 *
 * v2.5.2 (2026-04-05):
 * - Enabled UUID for msg_id consistency with MQTT msg_id tracking
 * - Binary packet fixed size to 512 bytes
 * - Code cleanup: removed inline comments
 *
 * v2.5.1 (2026-04-04):
 * - Added binary protocol support (COBS framing, CRC16-CCITT)
 * - Added dual-mode detection (AT commands + binary protocol)
 * - Added message ID correlation for async operations
 * - Added binary events (TX_QUEUED, TX_START, TX_DONE, TX_FAILED)
 * - 100% backward compatible with AT command mode
 */

#endif // VERSION_H
