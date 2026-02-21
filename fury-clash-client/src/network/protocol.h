#pragma once
#include <stdint.h>

/* ── Opcodes ─────────────────────────────────────────────────────────
 *  Values mirror the server's MsgType enum in types.ts exactly.
 *
 *  Client → Server
 */
typedef enum MsgType {
    MSG_JOIN        = 0x01,   /* 1B type + variable JSON payload          */
    MSG_INPUT       = 0x02,   /* 9-byte binary (see MsgInput below)       */
    MSG_PONG        = 0x03,   /* 1B type + 8B timestamp echo              */
    MSG_LOADOUT     = 0x04,   /* 2-byte: type + loadout (0/1/2)           */

    /* Server → Client */
    MSG_WAITING     = 0x10,   /* waiting for opponent (JSON)              */
    MSG_START       = 0x11,   /* match starts; JSON with slot/fighter IDs */
    MSG_INPUT_BATCH = 0x12,   /* relay batch of opponent inputs           */
    MSG_MATCH_END   = 0x13,   /* match over (JSON)                        */
    MSG_PING        = 0x14,   /* keep-alive; 1B type + 8B server timestamp*/
    MSG_ERROR       = 0x15,   /* server error (JSON)                      */
    MSG_RECONNECT   = 0x16,   /* opponent reconnected (JSON)              */
    MSG_DISCONNECT  = 0x17,   /* opponent disconnected (JSON)             */
    MSG_SERVER_LOADOUT = 0x20, /* relay fwd: opponent's loadout (2 bytes) */
} MsgType;

/* ── Input bitfield (2 bytes) ────────────────────────────────────────
 *   bit 0: up        bit 4: light_punch    bit  8: special1
 *   bit 1: down      bit 5: heavy_punch    bit  9: special2
 *   bit 2: left      bit 6: light_kick     bit 10: super
 *   bit 3: right     bit 7: heavy_kick     bit 11: grab
 */
#define INPUT_UP          (1 << 0)
#define INPUT_DOWN        (1 << 1)
#define INPUT_LEFT        (1 << 2)
#define INPUT_RIGHT       (1 << 3)
#define INPUT_LIGHT_PUNCH (1 << 4)
#define INPUT_HEAVY_PUNCH (1 << 5)
#define INPUT_LIGHT_KICK  (1 << 6)
#define INPUT_HEAVY_KICK  (1 << 7)
#define INPUT_SPECIAL1    (1 << 8)
#define INPUT_SPECIAL2    (1 << 9)
#define INPUT_SUPER       (1 << 10)
#define INPUT_GRAB        (1 << 11)

#pragma pack(push, 1)

/* CLIENT_INPUT — 9 bytes: 1B type | 2B seq | 4B frame | 2B input */
typedef struct MsgInput {
    uint8_t  type;    /* MSG_INPUT */
    uint16_t seq;
    uint32_t frame;
    uint16_t input;
} MsgInput;

/* One entry inside a SERVER_RELAY batch */
typedef struct MsgInputEntry {
    uint32_t frame;
    uint16_t input;
} MsgInputEntry;        /* 6 bytes */

/* SERVER_RELAY — 4B header + 6B × count:
 *   1B type | 2B seq | 1B count | MsgInputEntry[count]
 *   Server may send up to 255 entries; we read what arrives.         */
typedef struct MsgInputBatch {
    uint8_t        type;   /* MSG_INPUT_BATCH */
    uint16_t       seq;
    uint8_t        count;
    MsgInputEntry  entries[32]; /* practical upper bound per frame */
} MsgInputBatch;

#pragma pack(pop)
