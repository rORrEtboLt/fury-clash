/*
 * net_client.c — WebSocket relay client using POSIX TCP sockets (RFC 6455).
 *
 * Zero external dependencies beyond SDL3 and standard POSIX headers.
 * All I/O is non-blocking; net_client_receive() is safe to call every frame.
 *
 * Connection lifecycle:
 *   net_client_configure() → net_client_connect() → net_client_receive() loop
 *
 * Server message handling:
 *   MSG_WAITING     (0x10) → NET_WAITING
 *   MSG_START       (0x11) → NET_READY  (match info filled)
 *   MSG_INPUT_BATCH (0x12) → rollback_confirm_remote() for each entry
 *   MSG_PING        (0x14) → echo CLIENT_PONG (0x03) back
 *   MSG_MATCH_END   (0x13) → NET_ENDED (end info filled)
 *   MSG_ERROR       (0x15) → NET_ERROR
 */

#include "net_client.h"
#include "protocol.h"
#include "rollback.h"
#include "net_stats.h"

#include <SDL3/SDL.h>

/* POSIX networking */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Constants ──────────────────────────────────────────────────────────── */
#define WS_RECV_BUF    65536
#define WS_SEND_BUF    4096
#define HTTP_RESP_MAX  2048

/* RFC 6455 WebSocket key — any valid base64(16 bytes).
 * A fixed key is fine for a game client: we don't need CSRF protection. */
static const char *WS_HANDSHAKE_KEY = "dGhlIHNhbXBsZSBub25jZQ==";

/* ── Internal handshake state ────────────────────────────────────────────── */
typedef enum {
    HS_IDLE      = 0,
    HS_TCP_CONN  = 1,   /* non-blocking connect() pending, waiting for writability */
    HS_HTTP_SENT = 2,   /* HTTP Upgrade request sent, reading server response */
    HS_OPEN      = 3,   /* WS fully established */
    HS_CLOSED    = 4,
} WsHsState;

/* ── Global state ────────────────────────────────────────────────────────── */
static struct {
    /* Socket */
    int         fd;
    WsHsState   hs;

    /* Parsed URL components (filled by net_client_configure) */
    char        url[256];
    char        host[128];
    int         port;
    char        path[128];

    /* Match credentials */
    char        room_id[64];
    char        player_id[64];
    int         slot;
    int         configured;

    /* Ring buffer for incoming data */
    uint8_t     rbuf[WS_RECV_BUF];
    int         rbuf_head;     /* bytes accumulated, not yet consumed */

    /* Public state */
    NetState    state;
    NetMatchInfo match_info;
    NetEndInfo   end_info;
    NetStats     stats;

    int         network_mode;
} g_ws;

/* ── URL parser ──────────────────────────────────────────────────────────── */
static int parse_ws_url(const char *url,
                        char *host, int hlen,
                        int  *port,
                        char *path, int plen)
{
    const char *p = url;
    if (strncmp(p, "ws://", 5) != 0) {
        SDL_Log("[net] URL must start with ws://: %s", url);
        return 0;
    }
    p += 5;

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    /* host and optional port */
    if (colon && (!slash || colon < slash)) {
        int n = (int)(colon - p);
        if (n <= 0 || n >= hlen) return 0;
        memcpy(host, p, n);
        host[n] = '\0';
        *port = atoi(colon + 1);
    } else {
        int end = slash ? (int)(slash - p) : (int)strlen(p);
        if (end <= 0 || end >= hlen) return 0;
        memcpy(host, p, end);
        host[end] = '\0';
        *port = 80;
    }

    /* path */
    if (slash) {
        snprintf(path, plen, "%s", slash);
    } else {
        snprintf(path, plen, "/");
    }
    return 1;
}

/* ── Minimal JSON integer extractor ─────────────────────────────────────── */
/* Scans for "key":N in a JSON string without a library. */
static int json_int(const char *json, const char *key, int def)
{
    char pat[80];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t') p++;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    if (*p < '0' || *p > '9') return def;
    int val = 0;
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; }
    return val * sign;
}

/* ── WS frame encoder (client → server, MUST be masked per RFC 6455 §5.3) ─ */
/* Returns number of bytes written into dst[].
 * dst must have at least payload_len + 14 bytes capacity. */
static int ws_encode_frame(uint8_t *dst, const void *payload, size_t plen)
{
    int i = 0;

    /* FIN=1, RSV=0, opcode=0x02 (binary) */
    dst[i++] = 0x82;

    /* MASK bit set + payload length (7-bit, 16-bit, or 64-bit extended) */
    if (plen <= 125) {
        dst[i++] = (uint8_t)(0x80 | plen);
    } else if (plen <= 65535) {
        dst[i++] = 0x80 | 126;
        dst[i++] = (uint8_t)(plen >> 8);
        dst[i++] = (uint8_t)(plen & 0xFF);
    } else {
        dst[i++] = 0x80 | 127;
        /* 8-byte big-endian extended length */
        dst[i++] = 0; dst[i++] = 0; dst[i++] = 0; dst[i++] = 0;
        dst[i++] = (uint8_t)(plen >> 24);
        dst[i++] = (uint8_t)(plen >> 16);
        dst[i++] = (uint8_t)(plen >>  8);
        dst[i++] = (uint8_t)(plen & 0xFF);
    }

    /* 4-byte masking key — use SDL tick + a quick xorshift for variety */
    uint32_t r = (uint32_t)SDL_GetTicks();
    r ^= (r << 13); r ^= (r >> 17); r ^= (r << 5);   /* xorshift32 */
    uint8_t mask[4];
    mask[0] = (uint8_t)(r >> 24);
    mask[1] = (uint8_t)(r >> 16);
    mask[2] = (uint8_t)(r >>  8);
    mask[3] = (uint8_t)(r & 0xFF);
    memcpy(dst + i, mask, 4);
    i += 4;

    /* Masked payload */
    const uint8_t *src = (const uint8_t *)payload;
    for (size_t j = 0; j < plen; j++) {
        dst[i++] = src[j] ^ mask[j & 3];
    }
    return i;
}

/* ── Low-level WS send (wraps payload in a masked binary frame) ──────────── */
static void ws_send(const void *payload, size_t plen)
{
    if (g_ws.fd < 0 || g_ws.hs != HS_OPEN) return;

    uint8_t frame[WS_SEND_BUF + 16];
    if (plen + 16 > sizeof(frame)) {
        SDL_Log("[net] ws_send: payload too large (%zu bytes)", plen);
        return;
    }

    int flen = ws_encode_frame(frame, payload, plen);

    /* Send loop handles partial writes (rare but possible on congested links) */
    int sent = 0;
    while (sent < flen) {
        ssize_t n = send(g_ws.fd, frame + sent, (size_t)(flen - sent), MSG_NOSIGNAL);
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                /* Unlikely for small packets; just skip remaining bytes */
                break;
            }
            SDL_Log("[net] ws_send error: %s", strerror(errno));
            g_ws.state = NET_ERROR;
            return;
        }
        sent += (int)n;
    }
}

/* ── Send HTTP Upgrade request ───────────────────────────────────────────── */
static void send_http_upgrade(void)
{
    char req[512];
    int len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        g_ws.path, g_ws.host, g_ws.port, WS_HANDSHAKE_KEY);

    ssize_t sent = send(g_ws.fd, req, len, MSG_NOSIGNAL);
    if (sent < 0) {
        SDL_Log("[net] HTTP upgrade send failed: %s", strerror(errno));
        g_ws.state = NET_ERROR;
    }
}

/* ── Send CLIENT_JOIN immediately after WS handshake ─────────────────────── */
static void send_client_join(void)
{
    char json[512];
    int jlen = snprintf(json, sizeof(json),
        "{\"roomId\":\"%s\",\"playerId\":\"%s\",\"token\":\"dev\",\"slot\":%d}",
        g_ws.room_id, g_ws.player_id, g_ws.slot);

    size_t plen = (size_t)(1 + jlen);
    uint8_t *payload = (uint8_t *)malloc(plen);
    if (!payload) return;
    payload[0] = MSG_JOIN;
    memcpy(payload + 1, json, (size_t)jlen);
    ws_send(payload, plen);
    free(payload);

    SDL_Log("[net] CLIENT_JOIN sent for room=%s player=%s slot=%d",
            g_ws.room_id, g_ws.player_id, g_ws.slot);
}

/* ── Parse one complete WS frame from the receive buffer ─────────────────── */
/* Returns bytes consumed (>=1), 0 if incomplete, -1 on protocol error.
 * On success, *opcode and *payload / *plen are set to the frame's data.
 * The payload pointer points into g_ws.rbuf — copy it before consuming. */
static int ws_parse_frame(int *opcode, uint8_t **payload, size_t *plen)
{
    if (g_ws.rbuf_head < 2) return 0;

    uint8_t *b     = g_ws.rbuf;
    *opcode        = b[0] & 0x0F;
    int     masked = (b[1] >> 7) & 1;
    size_t  len    = b[1] & 0x7F;
    int     hdr    = 2;

    if (len == 126) {
        if (g_ws.rbuf_head < 4) return 0;
        len = ((size_t)b[2] << 8) | b[3];
        hdr = 4;
    } else if (len == 127) {
        if (g_ws.rbuf_head < 10) return 0;
        len = 0;
        for (int k = 2; k < 10; k++) len = (len << 8) | b[k];
        hdr = 10;
    }

    if (masked) hdr += 4;   /* server must NOT mask, but handle gracefully */

    size_t total = (size_t)hdr + len;
    if ((int)total > g_ws.rbuf_head) return 0;   /* wait for more data */
    if (total > WS_RECV_BUF)         return -1;  /* absurdly large frame */

    /* Unmask in-place if server sent masked data (non-standard) */
    if (masked) {
        uint8_t *mk = b + hdr - 4;
        for (size_t j = 0; j < len; j++) b[hdr + j] ^= mk[j & 3];
    }

    *payload = b + hdr;
    *plen    = len;
    return (int)total;
}

/* ── Dispatch a decoded application-level message ────────────────────────── */
static void dispatch_message(const uint8_t *payload, size_t plen)
{
    if (plen < 1) return;
    uint8_t type = payload[0];

    switch (type) {

    case MSG_WAITING:   /* 0x10 — server waiting for the other player */
        SDL_Log("[net] Waiting for opponent...");
        g_ws.state = NET_WAITING;
        break;

    case MSG_START: {   /* 0x11 — both players connected, match begins */
        /* payload = [0x11][JSON...] */
        const char *json = (const char *)(payload + 1);
        int json_len = (int)(plen - 1);
        SDL_Log("[net] SERVER_START: %.*s", json_len, json);

        g_ws.match_info.your_slot  = json_int(json, "yourSlot",  0);
        g_ws.match_info.p1_fighter = json_int(json, "p1Fighter", 0);
        g_ws.match_info.p2_fighter = json_int(json, "p2Fighter", 1);

        /* Extract roomId string */
        const char *ri = strstr(json, "\"roomId\":\"");
        if (ri) {
            ri += 10;
            const char *end = strchr(ri, '"');
            if (end) {
                int rlen = (int)(end - ri);
                if (rlen > 63) rlen = 63;
                memcpy(g_ws.match_info.room_id, ri, rlen);
                g_ws.match_info.room_id[rlen] = '\0';
            }
        }

        g_ws.state = NET_READY;
        SDL_Log("[net] Match ready: slot=%d fighters=%d vs %d",
                g_ws.match_info.your_slot,
                g_ws.match_info.p1_fighter,
                g_ws.match_info.p2_fighter);
        break;
    }

    case MSG_INPUT_BATCH: {   /* 0x12 — relay batch of opponent inputs */
        /* [0x12][seq_lo][seq_hi][count][frame0..3][input0..1] × count */
        if (plen < 4) break;
        uint8_t count = payload[3];
        size_t  need  = 4 + (size_t)count * 6;
        if (plen < need) break;

        const uint8_t *p = payload + 4;
        for (int i = 0; i < count; i++, p += 6) {
            uint32_t frame = (uint32_t)(p[0] | ((uint32_t)p[1] << 8) |
                                         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
            uint16_t input = (uint16_t)(p[4] | ((uint16_t)p[5] << 8));
            rollback_confirm_remote((int)frame, input);
        }
        break;
    }

    case MSG_PING: {    /* 0x14 — game-level keep-alive; echo back as PONG */
        /* payload = [0x14][8B server timestamp] */
        if (plen < 9) break;
        uint8_t pong[9];
        pong[0] = MSG_PONG;
        memcpy(pong + 1, payload + 1, 8);
        ws_send(pong, 9);
        /* RTT is tracked server-side; we can't easily measure it here
         * without synchronized clocks. Display remains at 0ms for now. */
        break;
    }

    case MSG_MATCH_END: {   /* 0x13 — match over */
        const char *json = (const char *)(payload + 1);
        int json_len = (int)(plen - 1);
        SDL_Log("[net] SERVER_END: %.*s", json_len, json);

        g_ws.end_info.winner_slot   = json_int(json, "winnerSlot",   -1);
        g_ws.end_info.p1_rounds_won = json_int(json, "p1RoundsWon",   0);
        g_ws.end_info.p2_rounds_won = json_int(json, "p2RoundsWon",   0);
        g_ws.state = NET_ENDED;
        break;
    }

    case MSG_ERROR: {   /* 0x15 — server error */
        const char *json = (const char *)(payload + 1);
        int json_len = (int)(plen - 1);
        SDL_Log("[net] SERVER_ERROR: %.*s", json_len, json);
        g_ws.state = NET_ERROR;
        break;
    }

    case MSG_DISCONNECT:    /* 0x17 — opponent disconnected (server will send END soon) */
        SDL_Log("[net] Opponent disconnected — waiting for match end...");
        break;

    case MSG_RECONNECT:     /* 0x16 — opponent reconnected */
        SDL_Log("[net] Opponent reconnected");
        break;

    default:
        SDL_Log("[net] Unknown message type 0x%02X (len=%zu)", type, plen);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

void net_client_configure(const char *url, const char *room_id,
                          const char *player_id, int slot)
{
    memset(&g_ws, 0, sizeof(g_ws));
    g_ws.fd           = -1;
    g_ws.hs           = HS_IDLE;
    g_ws.state        = NET_IDLE;
    g_ws.network_mode = 1;

    snprintf(g_ws.url,       sizeof(g_ws.url),       "%s", url       ? url       : "");
    snprintf(g_ws.room_id,   sizeof(g_ws.room_id),   "%s", room_id   ? room_id   : "");
    snprintf(g_ws.player_id, sizeof(g_ws.player_id), "%s", player_id ? player_id : "");
    g_ws.slot        = slot;
    g_ws.configured  = 1;

    if (url && !parse_ws_url(url, g_ws.host, sizeof(g_ws.host),
                             &g_ws.port, g_ws.path, sizeof(g_ws.path))) {
        SDL_Log("[net] Invalid URL: %s", url);
        g_ws.state = NET_ERROR;
    }

    net_stats_init(&g_ws.stats);
}

int net_client_connect(const char *url)
{
    /* If not configured yet (legacy call), configure now with just the URL */
    if (!g_ws.configured) {
        net_client_configure(url, "", "", 0);
    } else if (url && *url) {
        /* Allow overriding the URL at connect time */
        if (!parse_ws_url(url, g_ws.host, sizeof(g_ws.host),
                          &g_ws.port, g_ws.path, sizeof(g_ws.path))) {
            SDL_Log("[net] Invalid URL: %s", url);
            g_ws.state = NET_ERROR;
            return 0;
        }
    }

    if (g_ws.state == NET_ERROR) return 0;
    if (g_ws.fd >= 0) return 1;   /* already connected */

    /* DNS resolution (blocking — acceptable once per session) */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", g_ws.port);

    if (getaddrinfo(g_ws.host, port_str, &hints, &res) != 0 || !res) {
        SDL_Log("[net] DNS lookup failed for host: %s", g_ws.host);
        g_ws.state = NET_ERROR;
        return 0;
    }

    g_ws.fd = (int)socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (g_ws.fd < 0) {
        SDL_Log("[net] socket() failed: %s", strerror(errno));
        freeaddrinfo(res);
        g_ws.state = NET_ERROR;
        return 0;
    }

    /* Non-blocking mode */
    int flags = fcntl(g_ws.fd, F_GETFL, 0);
    fcntl(g_ws.fd, F_SETFL, flags | O_NONBLOCK);

    /* Disable Nagle algorithm — we send many small 9-byte packets */
    int one = 1;
    setsockopt(g_ws.fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    int rc = connect(g_ws.fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    g_ws.state = NET_CONNECTING;

    if (rc == 0) {
        /* Connected immediately (loopback) */
        send_http_upgrade();
        g_ws.hs = HS_HTTP_SENT;
    } else if (errno == EINPROGRESS) {
        /* Normal non-blocking path */
        g_ws.hs = HS_TCP_CONN;
    } else {
        SDL_Log("[net] connect() failed: %s", strerror(errno));
        close(g_ws.fd);
        g_ws.fd    = -1;
        g_ws.state = NET_ERROR;
        return 0;
    }

    SDL_Log("[net] Connecting to %s:%d%s", g_ws.host, g_ws.port, g_ws.path);
    return 1;
}

void net_client_receive(void)
{
    /* g_ws.fd is zero-initialized (= stdin) until net_client_configure() is
     * called.  Guard on configured so we never call recv() on fd 0. */
    if (!g_ws.configured || g_ws.fd < 0) return;

    /* ── Step 1: Wait for non-blocking connect() to complete ─────────────── */
    if (g_ws.hs == HS_TCP_CONN) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET((unsigned)g_ws.fd, &wfds);
        struct timeval tv = {0, 0};   /* poll: return immediately */
        int rc = select(g_ws.fd + 1, NULL, &wfds, NULL, &tv);
        if (rc > 0) {
            int err = 0;
            socklen_t elen = sizeof(err);
            getsockopt(g_ws.fd, SOL_SOCKET, SO_ERROR, &err, &elen);
            if (err != 0) {
                SDL_Log("[net] TCP connect failed: %s", strerror(err));
                g_ws.state = NET_ERROR;
                return;
            }
            send_http_upgrade();
            g_ws.hs = HS_HTTP_SENT;
        }
        /* else: still connecting — try again next frame */
        return;
    }

    /* ── Step 2: Read available bytes into ring buffer ───────────────────── */
    int avail = WS_RECV_BUF - g_ws.rbuf_head;
    if (avail <= 0) {
        SDL_Log("[net] Receive buffer full — resetting");
        g_ws.rbuf_head = 0;
        return;
    }

    ssize_t n = recv(g_ws.fd, g_ws.rbuf + g_ws.rbuf_head, (size_t)avail, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;   /* no data yet */
        SDL_Log("[net] recv() error: %s", strerror(errno));
        g_ws.state = NET_ERROR;
        return;
    }
    if (n == 0) {
        SDL_Log("[net] Server closed connection");
        g_ws.state = NET_ERROR;
        return;
    }
    g_ws.rbuf_head += (int)n;

    /* ── Step 3: Process HTTP 101 Upgrade response ───────────────────────── */
    if (g_ws.hs == HS_HTTP_SENT) {
        /* Look for end of HTTP headers: \r\n\r\n */
        int hdr_end = -1;
        for (int i = 0; i <= g_ws.rbuf_head - 4; i++) {
            if (g_ws.rbuf[i]   == '\r' && g_ws.rbuf[i+1] == '\n' &&
                g_ws.rbuf[i+2] == '\r' && g_ws.rbuf[i+3] == '\n') {
                hdr_end = i + 4;
                break;
            }
        }
        if (hdr_end < 0) return;   /* headers not yet complete */

        /* Verify 101 status */
        int got101 = 0;
        for (int i = 0; i <= hdr_end - 3; i++) {
            if (g_ws.rbuf[i] == '1' && g_ws.rbuf[i+1] == '0' && g_ws.rbuf[i+2] == '1') {
                got101 = 1; break;
            }
        }
        if (!got101) {
            SDL_Log("[net] WS upgrade failed (no 101): %.80s", (char *)g_ws.rbuf);
            g_ws.state = NET_ERROR;
            return;
        }

        /* Discard HTTP headers; keep any bytes after them (WS frames) */
        int leftover = g_ws.rbuf_head - hdr_end;
        if (leftover > 0) memmove(g_ws.rbuf, g_ws.rbuf + hdr_end, (size_t)leftover);
        g_ws.rbuf_head = leftover;
        g_ws.hs        = HS_OPEN;

        /* Immediately send CLIENT_JOIN */
        send_client_join();
        g_ws.state = NET_WAITING;
        SDL_Log("[net] WebSocket open");
        /* Fall through to process any WS frames already in the buffer */
    }

    /* ── Step 4: Parse and dispatch WS frames ────────────────────────────── */
    if (g_ws.hs != HS_OPEN) return;

    while (g_ws.rbuf_head > 0) {
        int      opcode;
        uint8_t *payload;
        size_t   plen;

        int consumed = ws_parse_frame(&opcode, &payload, &plen);
        if (consumed == 0) break;    /* incomplete frame — wait for more data */
        if (consumed < 0) {
            SDL_Log("[net] WS protocol error — closing");
            g_ws.state = NET_ERROR;
            break;
        }

        switch (opcode) {
        case 0x01:   /* text frame — treat same as binary */
        case 0x02:   /* binary frame */
            dispatch_message(payload, plen);
            break;
        case 0x08:   /* connection close */
            SDL_Log("[net] WS close frame received (code=%d)",
                    plen >= 2 ? (payload[0] << 8 | payload[1]) : 0);
            g_ws.state = NET_ERROR;
            break;
        case 0x09: { /* WS-level ping — respond with WS-level pong */
            uint8_t pong_frame[2 + 4 + 125] = {0x8A, 0x80};
            /* Pong with empty body (masked, MASK=1, len=0) */
            pong_frame[0] = 0x8A;
            pong_frame[1] = 0x80;
            pong_frame[2] = 0; pong_frame[3] = 0;
            pong_frame[4] = 0; pong_frame[5] = 0;
            send(g_ws.fd, pong_frame, 6, MSG_NOSIGNAL);
            break;
        }
        case 0x0A:   /* WS-level pong — ignore */
            break;
        default:
            SDL_Log("[net] Unknown WS opcode 0x%02X", opcode);
            break;
        }

        /* Slide consumed bytes out of the buffer */
        int leftover = g_ws.rbuf_head - consumed;
        if (leftover > 0) memmove(g_ws.rbuf, g_ws.rbuf + consumed, (size_t)leftover);
        g_ws.rbuf_head = leftover;

        if (g_ws.state == NET_ERROR) break;
    }
}

void net_client_send_input(int frame, uint16_t input)
{
    if (g_ws.state != NET_FIGHTING && g_ws.state != NET_READY) return;
    if (g_ws.hs != HS_OPEN) return;

    /* CLIENT_INPUT wire format: [0x02][seq_lo][seq_hi][f0][f1][f2][f3][i_lo][i_hi]
     * All multi-byte values are little-endian. */
    uint16_t seq = (uint16_t)(frame & 0xFFFF);
    uint8_t  buf[9];
    buf[0] = MSG_INPUT;
    buf[1] = (uint8_t)(seq & 0xFF);
    buf[2] = (uint8_t)(seq >> 8);
    buf[3] = (uint8_t)((uint32_t)frame & 0xFF);
    buf[4] = (uint8_t)(((uint32_t)frame >>  8) & 0xFF);
    buf[5] = (uint8_t)(((uint32_t)frame >> 16) & 0xFF);
    buf[6] = (uint8_t)(((uint32_t)frame >> 24) & 0xFF);
    buf[7] = (uint8_t)(input & 0xFF);
    buf[8] = (uint8_t)(input >> 8);

    ws_send(buf, 9);
}

void net_client_ack_start(void)
{
    if (g_ws.state == NET_READY) g_ws.state = NET_FIGHTING;
}

void net_client_disconnect(void)
{
    if (g_ws.fd >= 0) {
        /* Send WS close frame: FIN=1 opcode=8, MASK=1, len=0, empty mask key */
        uint8_t close_frame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00};
        send(g_ws.fd, close_frame, 6, MSG_NOSIGNAL);
        close(g_ws.fd);
        g_ws.fd = -1;
    }
    g_ws.hs    = HS_CLOSED;
    g_ws.state = NET_IDLE;
}

void net_client_shutdown(void)
{
    net_client_disconnect();
}

/* ── Query functions ─────────────────────────────────────────────────────── */

NetState net_client_get_state(void)           { return g_ws.state; }
int      net_client_is_connected(void)        { return g_ws.state >= NET_WAITING &&
                                                        g_ws.state <= NET_FIGHTING; }
float    net_client_ping_ms(void)             { return g_ws.stats.ping_ms; }
int      net_client_is_network_mode(void)     { return g_ws.network_mode; }

const NetMatchInfo *net_client_get_match_info(void) { return &g_ws.match_info; }
const NetEndInfo   *net_client_get_end_info(void)   { return &g_ws.end_info; }
