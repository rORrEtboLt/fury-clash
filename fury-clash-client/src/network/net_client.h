#pragma once
#include <stdint.h>

/* ── Connection & match lifecycle states ────────────────────────────────── */
typedef enum NetState {
    NET_IDLE        = 0,  /* not connected */
    NET_CONNECTING  = 1,  /* TCP handshake + WS upgrade in progress */
    NET_WAITING     = 2,  /* WS open, CLIENT_JOIN sent, waiting for opponent */
    NET_READY       = 3,  /* SERVER_START received — fight can begin */
    NET_FIGHTING    = 4,  /* in-match: relaying inputs */
    NET_ENDED       = 5,  /* SERVER_END received */
    NET_ERROR       = 6,  /* unrecoverable error */
} NetState;

/* Info filled in when SERVER_START arrives */
typedef struct NetMatchInfo {
    int  your_slot;       /* 0 or 1 */
    int  p1_fighter;
    int  p2_fighter;
    char room_id[64];
} NetMatchInfo;

/* Match end info from SERVER_END */
typedef struct NetEndInfo {
    int  winner_slot;     /* -1 = draw */
    int  p1_rounds_won;
    int  p2_rounds_won;
} NetEndInfo;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/* Store connection params before connecting.
 * Must be called before net_client_connect() in network mode. */
void net_client_configure(const char *url, const char *room_id,
                          const char *player_id, int slot);

/* Open TCP socket and start WS handshake (non-blocking).
 * url may be NULL if net_client_configure() was already called. */
int  net_client_connect(const char *url);

void net_client_disconnect(void);
void net_client_shutdown(void);

/* ── Per-frame ──────────────────────────────────────────────────────────── */

/* Poll WS for incoming frames; feeds rollback_confirm_remote() for opponent inputs.
 * MUST be called every frame regardless of state. */
void net_client_receive(void);

/* Send CLIENT_INPUT (9-byte masked WS binary frame). */
void net_client_send_input(int frame, uint16_t input);

/* Send CLIENT_LOADOUT (2-byte: type + loadout byte 0=none/1=fire/2=frost). */
void net_client_send_loadout(int loadout);

/* Returns the opponent's loadout (0/1/2) once SERVER_LOADOUT arrives, or -1. */
int net_client_get_opponent_loadout(void);

/* ── State transitions ──────────────────────────────────────────────────── */

/* Call after reading NET_READY to move state to NET_FIGHTING. */
void net_client_ack_start(void);

/* ── Query ──────────────────────────────────────────────────────────────── */

NetState              net_client_get_state(void);
int                   net_client_is_connected(void);   /* state >= NET_WAITING */
float                 net_client_ping_ms(void);
const NetMatchInfo   *net_client_get_match_info(void);
const NetEndInfo     *net_client_get_end_info(void);
int                   net_client_is_network_mode(void);
