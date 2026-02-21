/*
 * Fury Clash — main.c
 * Entry point: initialise SDL3, hand off to game loop.
 *
 * Desktop (no args): lobby screen → create/join room → loadout selection
 * Desktop (argc==5): ws://HOST:PORT ROOM_ID PLAYER_ID SLOT  — skip lobby
 * iOS / FC_IOS_SIM:  lobby screen starts immediately on launch
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "core/gameloop.h"
#include "core/state_machine.h"
#include "platform/platform.h"
#include "render/renderer.h"
#include "input/input.h"
#include "network/net_client.h"
#include "network/http_client.h"
#include "audio/audio.h"
#include "gameplay/simulation.h"
#include "network/rollback.h"

/* ── Globals ────────────────────────────────────────────────────────────── */
static PlatformInfo g_platform;
static StateMachine g_state;
static GameLoop     g_loop;
static int          g_local_slot = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 * Lobby state machine
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum LobbySubState {
    LS_SETUP,        /* enter server URL                            */
    LS_ROLE,         /* choose: Create Room | Join Room | Local Play*/
    LS_CREATING,     /* HTTP POST /rooms in progress (brief block)  */
    LS_SHOW_CODE,    /* P1: display code; WS connecting/waiting     */
    LS_ENTER_CODE,   /* P2: type 6-char room code                   */
    LS_WAITING,      /* P2: WS connecting + waiting for SERVER_START*/
    LS_LOADOUT,      /* both: pick Fire / Frost                     */
    LS_READY,        /* both loadouts received → fight              */
} LobbySubState;

typedef enum LobbyField {
    LF_SERVER = 0,
    LF_CODE   = 1,
} LobbyField;

static struct {
    LobbySubState sub;
    char          server_url[128];  /* ws://host:port     */
    char          room_code[8];     /* 6-char code P1 generated */
    char          code_input[8];    /* P2's typed code    */
    int           is_p1;            /* 1 = we created the room */
    int           my_loadout;       /* 0=unset 1=fire 2=frost  */
    int           loadout_sent;     /* 1 once CLIENT_LOADOUT sent */
    int           local_mode;       /* 1 = local play, no network */
    LobbyField    active_field;
    int           pending_text_input; /* deferred SDL_StartTextInput */
    char          error_msg[128];
} g_lobby;

static void lobby_init(void)
{
    memset(&g_lobby, 0, sizeof(g_lobby));
    SDL_snprintf(g_lobby.server_url, sizeof(g_lobby.server_url),
                 "%s", FC_IOS_SERVER_URL);
    g_lobby.sub          = LS_SETUP;
    g_lobby.active_field = LF_SERVER;
}

static void lobby_field_append(char *buf, int maxlen, const char *text)
{
    int n    = (int)strlen(buf);
    int tlen = (int)strlen(text);
    if (n + tlen < maxlen - 1) {
        memcpy(buf + n, text, (size_t)tlen);
        buf[n + tlen] = '\0';
    }
}

static void lobby_field_backspace(char *buf)
{
    int n = (int)strlen(buf);
    if (n > 0) buf[n - 1] = '\0';
}

static void gen_room_code(char *out)
{
    /* 6 uppercase alphanumeric chars, no ambiguous I/O/0/1 */
    static const char ch[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 6; i++)
        out[i] = ch[SDL_rand(32)];
    out[6] = '\0';
}

/* Called once when P1 clicks "Create Room" */
static void lobby_create_room(void)
{
    gen_room_code(g_lobby.room_code);

    char body[256];
    SDL_snprintf(body, sizeof(body),
        "{\"roomId\":\"%s\",\"p1Id\":\"%sP1\",\"p2Id\":\"%sP2\","
        "\"p1Fighter\":0,\"p2Fighter\":0}",
        g_lobby.room_code, g_lobby.room_code, g_lobby.room_code);

    char host[128]; int port;
    if (!http_parse_ws_url(g_lobby.server_url, host, sizeof(host), &port)) {
        SDL_snprintf(g_lobby.error_msg, sizeof(g_lobby.error_msg),
                     "Invalid server URL");
        g_lobby.sub = LS_ROLE;
        return;
    }

    char resp[512];
    int status = http_post(host, port, "/rooms", body, resp, sizeof(resp));
    if (status == 200 || status == 201) {
        char player_id[16];
        SDL_snprintf(player_id, sizeof(player_id), "%sP1", g_lobby.room_code);
        net_client_configure(g_lobby.server_url,
                             g_lobby.room_code, player_id, 0);
        net_client_connect(NULL);
        g_lobby.is_p1 = 1;
        g_lobby.sub   = LS_SHOW_CODE;
        g_lobby.error_msg[0] = '\0';
        SDL_Log("[lobby] Room %s created (HTTP %d)", g_lobby.room_code, status);
    } else {
        SDL_snprintf(g_lobby.error_msg, sizeof(g_lobby.error_msg),
                     "Room creation failed (HTTP %d)", status);
        SDL_Log("[lobby] %s", g_lobby.error_msg);
        g_lobby.sub = LS_ROLE;
    }
}

/* Called when P2 clicks "Join" with a typed code */
static void lobby_join_room(void)
{
    if (g_lobby.code_input[0] == '\0') return;
    char player_id[16];
    SDL_snprintf(player_id, sizeof(player_id), "%sP2", g_lobby.code_input);
    /* copy code_input into room_code for unified access later */
    SDL_snprintf(g_lobby.room_code, sizeof(g_lobby.room_code),
                 "%s", g_lobby.code_input);
    net_client_configure(g_lobby.server_url,
                         g_lobby.room_code, player_id, 1);
    net_client_connect(NULL);
    g_lobby.is_p1 = 0;
    g_lobby.sub   = LS_WAITING;
    g_lobby.error_msg[0] = '\0';
    SDL_Log("[lobby] Joining room %s as P2", g_lobby.room_code);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Lobby render
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Draw a labelled text input box; returns 1 if active. */
static void draw_field(SDL_Renderer *ren, float x, float y, float w, float h,
                       const char *label, const char *value, int active)
{
    SDL_SetRenderDrawColor(ren,
        active ? 60 : 30, active ? 90 : 50, active ? 180 : 100, 255);
    SDL_FRect box = { x, y, w, h };
    SDL_RenderFillRect(ren, &box);
    SDL_SetRenderDrawColor(ren, active ? 160 : 80, active ? 200 : 120, 255, 255);
    SDL_RenderRect(ren, &box);
    SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
    SDL_RenderDebugTextFormat(ren, x + 10, y + 8, "%s", label);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    /* Blink cursor when active */
    const char *cursor = (active && (SDL_GetTicks() / 500) % 2) ? "|" : "";
    SDL_RenderDebugTextFormat(ren, x + 10, y + 28, "%s%s", value, cursor);
}

static void draw_button(SDL_Renderer *ren, float x, float y, float w, float h,
                        const char *label, Uint8 r, Uint8 g2, Uint8 b)
{
    SDL_SetRenderDrawColor(ren, r, g2, b, 255);
    SDL_FRect btn = { x, y, w, h };
    SDL_RenderFillRect(ren, &btn);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    /* Rough centre of button */
    float lw = (float)strlen(label) * 8.0f;
    SDL_RenderDebugTextFormat(ren, x + (w - lw) * 0.5f, y + h * 0.5f - 4, "%s", label);
}

static void render_lobby(SDL_Renderer *ren)
{
    /* Background */
    SDL_SetRenderDrawColor(ren, 12, 12, 30, 255);
    SDL_FRect bg = { 0, 0, FC_GAME_W, FC_GAME_H };
    SDL_RenderFillRect(ren, &bg);

    /* Title */
    SDL_SetRenderDrawColor(ren, 220, 180, 255, 255);
    SDL_RenderDebugTextFormat(ren, 520.0f, 40.0f, "FURY CLASH");
    SDL_SetRenderDrawColor(ren, 140, 140, 180, 255);
    SDL_RenderDebugTextFormat(ren, 490.0f, 65.0f, "LOBBY");

    /* Error message */
    if (g_lobby.error_msg[0]) {
        SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
        SDL_RenderDebugTextFormat(ren, 160.0f, 120.0f, "%s", g_lobby.error_msg);
    }

    switch (g_lobby.sub) {

    /* ── LS_SETUP: server URL entry ─────────────────────────────────────── */
    case LS_SETUP:
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 160.0f, 200.0f, "Server URL:");
        draw_field(ren, 160, 220, 960, 70,
                   "", g_lobby.server_url,
                   g_lobby.active_field == LF_SERVER);
        SDL_SetRenderDrawColor(ren, 160, 160, 160, 255);
        SDL_RenderDebugTextFormat(ren, 160.0f, 305.0f,
            "e.g.  ws://192.168.1.100:3002");
        draw_button(ren, 490, 380, 300, 70, "Next  ->", 50, 120, 200);
        break;

    /* ── LS_ROLE: pick P1 / P2 / Local ─────────────────────────────────── */
    case LS_ROLE:
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 420.0f, 220.0f, "Choose your role:");
        draw_button(ren, 100, 300, 340, 90, "Create Room  (P1)", 40, 100, 200);
        draw_button(ren, 480, 300, 340, 90, "Join Room   (P2)", 200, 80, 60);
        draw_button(ren, 860, 300, 320, 90, "Local Play", 50, 140, 60);
        SDL_SetRenderDrawColor(ren, 140, 140, 140, 255);
        SDL_RenderDebugTextFormat(ren, 160.0f, 430.0f,
            "Server: %s", g_lobby.server_url);
        break;

    /* ── LS_CREATING: brief wait during HTTP POST ───────────────────────── */
    case LS_CREATING: {
        int dots = (int)(SDL_GetTicks() / 300) % 4;
        char d[5] = "   ";
        for (int i = 0; i < dots; i++) d[i] = '.';
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 480.0f, 340.0f, "Creating room%s", d);
        break;
    }

    /* ── LS_SHOW_CODE: P1 shows the 6-char code ─────────────────────────── */
    case LS_SHOW_CODE: {
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 350.0f, 200.0f, "Share this code with P2:");

        /* Large code display */
        SDL_SetRenderDrawColor(ren, 30, 60, 120, 255);
        SDL_FRect codebox = { 280, 240, 720, 120 };
        SDL_RenderFillRect(ren, &codebox);
        SDL_SetRenderDrawColor(ren, 100, 180, 255, 255);
        SDL_RenderRect(ren, &codebox);
        SDL_SetRenderDrawColor(ren, 255, 255, 100, 255);
        /* 6 chars × ~18px wide with spacing = ~140px total; center in 720px box */
        for (int i = 0; i < 6 && g_lobby.room_code[i]; i++) {
            char ch[2] = { g_lobby.room_code[i], '\0' };
            SDL_RenderDebugTextFormat(ren, 370.0f + (float)i * 90.0f, 286.0f, "%s", ch);
        }

        /* WS status */
        NetState ns = net_client_get_state();
        SDL_SetRenderDrawColor(ren, 160, 200, 160, 255);
        if (ns == NET_WAITING) {
            int dots = (int)(SDL_GetTicks() / 400) % 4;
            char d[5] = "   "; for (int i = 0; i < dots; i++) d[i] = '.';
            SDL_RenderDebugTextFormat(ren, 390.0f, 400.0f,
                "Waiting for P2%s", d);
        } else if (ns == NET_ERROR) {
            SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
            SDL_RenderDebugTextFormat(ren, 400.0f, 400.0f, "Connection error");
        } else {
            SDL_RenderDebugTextFormat(ren, 430.0f, 400.0f, "Connecting...");
        }
        break;
    }

    /* ── LS_ENTER_CODE: P2 types the room code ──────────────────────────── */
    case LS_ENTER_CODE:
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 350.0f, 230.0f, "Enter the room code:");
        draw_field(ren, 340, 270, 600, 70,
                   "", g_lobby.code_input,
                   g_lobby.active_field == LF_CODE);
        draw_button(ren, 480, 390, 320, 70, "Join", 200, 80, 60);
        draw_button(ren, 120, 390, 200, 70, "Back", 60, 60, 80);
        break;

    /* ── LS_WAITING: P2 connecting + waiting ────────────────────────────── */
    case LS_WAITING: {
        NetState ns = net_client_get_state();
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        const char *line = "Connecting...";
        if (ns == NET_WAITING) line = "Waiting for P1 to start...";
        if (ns == NET_ERROR)   line = "Connection failed! Press ESC.";
        SDL_RenderDebugTextFormat(ren, 380.0f, 340.0f, "%s", line);
        if (ns == NET_ERROR) {
            SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
        }
        break;
    }

    /* ── LS_LOADOUT: pick fire / frost ──────────────────────────────────── */
    case LS_LOADOUT: {
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 390.0f, 190.0f,
            "Pick your pre-match loadout:");

        /* Fire button */
        int fire_picked  = (g_lobby.my_loadout == 1);
        SDL_SetRenderDrawColor(ren,
            fire_picked ? 255 : 180, fire_picked ? 120 : 60, 20, 255);
        SDL_FRect fbtn = { 160, 260, 380, 130 };
        SDL_RenderFillRect(ren, &fbtn);
        SDL_SetRenderDrawColor(ren, 255, 200, 80, 255);
        SDL_RenderRect(ren, &fbtn);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(ren, 250.0f, 300.0f, "FIRE PACK");
        SDL_SetRenderDrawColor(ren, 255, 180, 80, 255);
        SDL_RenderDebugTextFormat(ren, 186.0f, 330.0f, "+20%% damage dealt");

        /* Frost button */
        int frost_picked = (g_lobby.my_loadout == 2);
        SDL_SetRenderDrawColor(ren,
            frost_picked ? 80 : 40, frost_picked ? 180 : 100, frost_picked ? 255 : 180, 255);
        SDL_FRect cbtn = { 740, 260, 380, 130 };
        SDL_RenderFillRect(ren, &cbtn);
        SDL_SetRenderDrawColor(ren, 80, 220, 255, 255);
        SDL_RenderRect(ren, &cbtn);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(ren, 820.0f, 300.0f, "FROST PACK");
        SDL_SetRenderDrawColor(ren, 140, 220, 255, 255);
        SDL_RenderDebugTextFormat(ren, 760.0f, 330.0f, "+20 max HP");

        /* Your pick indicator */
        if (g_lobby.my_loadout != 0) {
            SDL_SetRenderDrawColor(ren, 100, 220, 100, 255);
            SDL_RenderDebugTextFormat(ren, 440.0f, 430.0f,
                "Your pick: %s",
                g_lobby.my_loadout == 1 ? "Fire Pack" : "Frost Pack");
        }

        /* Opponent status (network mode) */
        if (!g_lobby.local_mode) {
            int opp = net_client_get_opponent_loadout();
            if (opp < 0) {
                SDL_SetRenderDrawColor(ren, 160, 160, 160, 255);
                SDL_RenderDebugTextFormat(ren, 410.0f, 480.0f,
                    "Waiting for opponent...");
            } else {
                SDL_SetRenderDrawColor(ren, 100, 220, 100, 255);
                SDL_RenderDebugTextFormat(ren, 410.0f, 480.0f,
                    "Opponent: %s - Starting!",
                    opp == 1 ? "Fire Pack" : (opp == 2 ? "Frost Pack" : "None"));
            }
        }

        /* Local mode second player */
        if (g_lobby.local_mode && g_lobby.my_loadout != 0) {
            SDL_SetRenderDrawColor(ren, 200, 200, 100, 255);
            SDL_RenderDebugTextFormat(ren, 400.0f, 480.0f,
                "Loadout selected! Starting...");
        }
        break;
    }

    /* ── LS_READY ────────────────────────────────────────────────────────── */
    case LS_READY:
        SDL_SetRenderDrawColor(ren, 100, 255, 100, 255);
        SDL_RenderDebugTextFormat(ren, 510.0f, 340.0f, "FIGHT!");
        break;
    }

    /* Server URL shown at bottom except during setup */
    if (g_lobby.sub > LS_ROLE) {
        SDL_SetRenderDrawColor(ren, 80, 80, 100, 255);
        SDL_RenderDebugTextFormat(ren, 20.0f, FC_GAME_H - 24.0f,
            "%s", g_lobby.server_url);
    }
}

/* ── Lobby pointer/touch handler — called with logical game coords ────────── */
static void lobby_handle_touch(float tx, float ty)
{
    switch (g_lobby.sub) {

    case LS_SETUP:
        /* Server URL field */
        if (tx >= 160 && tx <= 1120 && ty >= 220 && ty <= 290) {
            g_lobby.active_field = LF_SERVER;
            SDL_StopTextInput(NULL);
            g_lobby.pending_text_input = 1;
            return;
        }
        /* Next button */
        if (tx >= 490 && tx <= 790 && ty >= 380 && ty <= 450) {
            if (g_lobby.server_url[0] != '\0') {
                g_lobby.sub = LS_ROLE;
                SDL_StopTextInput(NULL);
            }
        }
        break;

    case LS_ROLE:
        /* Create Room (P1) */
        if (tx >= 100 && tx <= 440 && ty >= 300 && ty <= 390) {
            g_lobby.sub = LS_CREATING;
        }
        /* Join Room (P2) */
        if (tx >= 480 && tx <= 820 && ty >= 300 && ty <= 390) {
            g_lobby.sub          = LS_ENTER_CODE;
            g_lobby.active_field = LF_CODE;
            SDL_StopTextInput(NULL);
            g_lobby.pending_text_input = 1;
        }
        /* Local Play */
        if (tx >= 860 && tx <= 1180 && ty >= 300 && ty <= 390) {
            g_lobby.local_mode = 1;
            g_lobby.sub        = LS_LOADOUT;
        }
        break;

    case LS_ENTER_CODE:
        /* Code input field */
        if (tx >= 340 && tx <= 940 && ty >= 270 && ty <= 340) {
            g_lobby.active_field = LF_CODE;
            SDL_StopTextInput(NULL);
            g_lobby.pending_text_input = 1;
            return;
        }
        /* Join button */
        if (tx >= 480 && tx <= 800 && ty >= 390 && ty <= 460) {
            SDL_StopTextInput(NULL);
            lobby_join_room();
        }
        /* Back button */
        if (tx >= 120 && tx <= 320 && ty >= 390 && ty <= 460) {
            g_lobby.sub = LS_ROLE;
            SDL_StopTextInput(NULL);
        }
        break;

    case LS_LOADOUT:
        /* Fire button */
        if (tx >= 160 && tx <= 540 && ty >= 260 && ty <= 390) {
            if (!g_lobby.loadout_sent) {
                g_lobby.my_loadout  = 1;
                if (!g_lobby.local_mode) {
                    net_client_send_loadout(1);
                    g_lobby.loadout_sent = 1;
                }
            }
        }
        /* Frost button */
        if (tx >= 740 && tx <= 1120 && ty >= 260 && ty <= 390) {
            if (!g_lobby.loadout_sent) {
                g_lobby.my_loadout  = 2;
                if (!g_lobby.local_mode) {
                    net_client_send_loadout(2);
                    g_lobby.loadout_sent = 1;
                }
            }
        }
        break;

    default:
        break;
    }
}

/* ── Lobby keyboard handler ───────────────────────────────────────────────── */
static void lobby_handle_key(SDL_Keycode key)
{
    char *active_buf = NULL;

    if (g_lobby.sub == LS_SETUP)
        active_buf = g_lobby.server_url;
    else if (g_lobby.sub == LS_ENTER_CODE)
        active_buf = g_lobby.code_input;

    if (key == SDLK_BACKSPACE && active_buf)
        lobby_field_backspace(active_buf);

    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        switch (g_lobby.sub) {
        case LS_SETUP:
            if (g_lobby.server_url[0]) {
                g_lobby.sub = LS_ROLE;
                SDL_StopTextInput(NULL);
            }
            break;
        case LS_ENTER_CODE:
            SDL_StopTextInput(NULL);
            lobby_join_room();
            break;
        default:
            break;
        }
    }

    if (key == SDLK_ESCAPE) {
        switch (g_lobby.sub) {
        case LS_ENTER_CODE:
        case LS_ROLE:
            g_lobby.sub = (g_lobby.sub == LS_ENTER_CODE) ? LS_ROLE : LS_SETUP;
            SDL_StopTextInput(NULL);
            break;
        case LS_SHOW_CODE:
        case LS_WAITING:
            net_client_disconnect();
            g_lobby.sub = LS_ROLE;
            break;
        default:
            break;
        }
    }

    /* Keyboard shortcuts for role selection */
    if (g_lobby.sub == LS_ROLE) {
        if (key == SDLK_1) { g_lobby.sub = LS_CREATING; }
        if (key == SDLK_2) {
            g_lobby.sub          = LS_ENTER_CODE;
            g_lobby.active_field = LF_CODE;
            SDL_StartTextInput(NULL);
        }
        if (key == SDLK_3) {
            g_lobby.local_mode = 1;
            g_lobby.sub        = LS_LOADOUT;
        }
    }

    /* Keyboard loadout selection */
    if (g_lobby.sub == LS_LOADOUT && !g_lobby.loadout_sent) {
        if (key == SDLK_F && g_lobby.my_loadout == 0) {
            g_lobby.my_loadout = 1;
            if (!g_lobby.local_mode) { net_client_send_loadout(1); g_lobby.loadout_sent = 1; }
        }
        if (key == SDLK_R && g_lobby.my_loadout == 0) {
            g_lobby.my_loadout = 2;
            if (!g_lobby.local_mode) { net_client_send_loadout(2); g_lobby.loadout_sent = 1; }
        }
    }
}

/* ── Lobby text input ─────────────────────────────────────────────────────── */
static void lobby_handle_text_input(const char *text)
{
    switch (g_lobby.sub) {
    case LS_SETUP:
        lobby_field_append(g_lobby.server_url, (int)sizeof(g_lobby.server_url), text);
        break;
    case LS_ENTER_CODE:
        /* Code is exactly 6 uppercase chars */
        if (strlen(g_lobby.code_input) < 6) {
            char upper[16];
            int i = 0;
            while (text[i] && i < (int)sizeof(upper) - 1) {
                char c = text[i];
                if (c >= 'a' && c <= 'z') c -= 32;
                upper[i++] = c;
            }
            upper[i] = '\0';
            lobby_field_append(g_lobby.code_input, (int)sizeof(g_lobby.code_input), upper);
        }
        break;
    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Existing screen render helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static void render_title(SDL_Renderer *ren)
{
    SDL_SetRenderDrawColor(ren, 60, 20, 80, 220);
    SDL_FRect card = { 340.0f, 270.0f, 600.0f, 90.0f };
    SDL_RenderFillRect(ren, &card);
    SDL_SetRenderDrawColor(ren, 200, 100, 255, 255);
    SDL_RenderRect(ren, &card);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 460.0f, 306.0f, "FURY  CLASH");

    if ((SDL_GetTicks() / 500) % 2) {
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 440.0f, 420.0f, "Press ENTER to play");
    }
}

static void render_searching(SDL_Renderer *ren)
{
    NetState ns = net_client_get_state();

    SDL_SetRenderDrawColor(ren, 10, 20, 60, 220);
    SDL_FRect card = { 240.0f, 280.0f, 800.0f, 160.0f };
    SDL_RenderFillRect(ren, &card);
    SDL_SetRenderDrawColor(ren, 80, 120, 255, 255);
    SDL_RenderRect(ren, &card);

    const char *line = "Connecting...";
    if (ns == NET_WAITING) line = "Waiting for opponent...";
    if (ns == NET_ERROR)   line = "Connection failed!  Press ESC to go back.";

    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 300.0f, 320.0f, "%s", line);

    if (ns != NET_ERROR) {
        int dots = (int)(SDL_GetTicks() / 300) % 4;
        char d[5] = "    ";
        for (int i = 0; i < dots; i++) d[i] = '.';
        SDL_RenderDebugTextFormat(ren, 300.0f, 360.0f, "%s", d);
    }

    if (ns >= NET_WAITING) {
        SDL_SetRenderDrawColor(ren, 150, 200, 150, 255);
        SDL_RenderDebugTextFormat(ren, 300.0f, 390.0f,
            "Ping: %.0f ms", net_client_ping_ms());
    }
}

static void render_results(SDL_Renderer *ren)
{
    const NetEndInfo *ei = net_client_get_end_info();
    SDL_SetRenderDrawColor(ren, 40, 20, 20, 220);
    SDL_FRect card = { 290.0f, 270.0f, 700.0f, 180.0f };
    SDL_RenderFillRect(ren, &card);
    SDL_SetRenderDrawColor(ren, 255, 180, 50, 255);
    SDL_RenderRect(ren, &card);

    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    if (net_client_is_network_mode() && ei) {
        const char *result;
        if (ei->winner_slot == -1)               result = "DRAW!";
        else if (ei->winner_slot == g_local_slot) result = "YOU WIN!";
        else                                      result = "YOU LOSE!";
        SDL_RenderDebugTextFormat(ren, 400.0f, 310.0f, "%s", result);
        SDL_RenderDebugTextFormat(ren, 350.0f, 360.0f,
            "P1 Rounds: %d    P2 Rounds: %d",
            ei->p1_rounds_won, ei->p2_rounds_won);
    } else {
        SDL_RenderDebugTextFormat(ren, 400.0f, 310.0f, "MATCH OVER");
    }

    if ((SDL_GetTicks() / 600) % 2) {
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDebugTextFormat(ren, 430.0f, 410.0f, "Press ENTER to continue");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    int cli_mode = 0;   /* 1 when using argc==5 legacy args */

    /* ── Parse arguments ──────────────────────────────────────────────── */
#ifdef FC_PLATFORM_IOS
    (void)argc; (void)argv;
#else
    if (argc == 5) {
        const char *ws_url    = argv[1];
        const char *room_id   = argv[2];
        const char *player_id = argv[3];
        int         slot      = atoi(argv[4]);
        if (slot != 0 && slot != 1) {
            SDL_Log("SLOT must be 0 or 1"); return 1;
        }
        net_client_configure(ws_url, room_id, player_id, slot);
        g_local_slot = slot;
        cli_mode     = 1;
    } else if (argc != 1) {
        SDL_Log("Usage: %s [ws://HOST:PORT ROOM_ID PLAYER_ID SLOT]", argv[0]);
        return 1;
    }
#endif

    /* ── SDL3 init ────────────────────────────────────────────────────── */
#ifdef FC_IOS_SIM
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    platform_init(&g_platform);

    SDL_Window *window = SDL_CreateWindow(
        "Fury Clash",
#if defined(FC_IOS_SIM)
        FC_IOSSIM_W, FC_IOSSIM_H,
        SDL_WINDOW_HIGH_PIXEL_DENSITY
#elif defined(FC_PLATFORM_IOS)
        FC_GAME_W, FC_GAME_H,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
#else
        FC_GAME_W, FC_GAME_H,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
#endif
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!renderer_init(window, &g_platform)) {
        SDL_Log("renderer_init failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    audio_init();
    input_init();
    statemachine_init(&g_state);
    gameloop_init(&g_loop);
    rollback_init();
    lobby_init();

    /* ── Initial state ────────────────────────────────────────────────── */
    if (cli_mode) {
        /* Legacy: skip lobby, connect and go directly to match search */
        net_client_connect(NULL);
        statemachine_request(&g_state, GS_MATCH_SEARCH);
    } else {
        /* All builds: start at lobby */
#ifdef FC_PLATFORM_IOS
        /* On iOS, skip LS_SETUP if FC_IOS_SERVER_URL is pre-filled */
        if (g_lobby.server_url[0] != '\0') {
            g_lobby.sub = LS_ROLE;
        }
#endif
        statemachine_request(&g_state, GS_LOBBY);
        /* Start text input for URL field on desktop */
#ifndef FC_PLATFORM_IOS
        if (g_lobby.sub == LS_SETUP)
            SDL_StartTextInput(NULL);
#endif
    }

    /* ── Main loop ────────────────────────────────────────────────────── */
    while (g_loop.running) {

        /* Deferred keyboard activation (iOS: stop+start must span frames) */
#ifdef FC_PLATFORM_IOS
        if (g_lobby.pending_text_input && g_state.current == GS_LOBBY) {
            g_lobby.pending_text_input = 0;
            SDL_StartTextInput(NULL);
        }
#endif

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT)
                g_loop.running = 0;

            /* ── Lobby events ─────────────────────────────────────────── */
            if (g_state.current == GS_LOBBY) {

                /* Touch/click → lobby_handle_touch (logical game coords) */
#ifdef FC_PLATFORM_IOS
                if (ev.type == SDL_EVENT_FINGER_DOWN) {
                    int ww, wh;
                    SDL_GetWindowSize(window, &ww, &wh);
                    float tx, ty;
                    SDL_RenderCoordinatesFromWindow(renderer_get(),
                        ev.tfinger.x * (float)ww,
                        ev.tfinger.y * (float)wh,
                        &tx, &ty);
                    lobby_handle_touch(tx, ty);
                }
#else
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    ev.button.button == SDL_BUTTON_LEFT) {
                    float tx, ty;
                    SDL_RenderCoordinatesFromWindow(renderer_get(),
                        ev.button.x, ev.button.y, &tx, &ty);
                    lobby_handle_touch(tx, ty);
                    /* Ensure text input is active after clicking a field */
                    if (g_lobby.sub == LS_SETUP || g_lobby.sub == LS_ENTER_CODE)
                        SDL_StartTextInput(NULL);
                }
#endif
                if (ev.type == SDL_EVENT_TEXT_INPUT) {
                    lobby_handle_text_input(ev.text.text);
                }
                if (ev.type == SDL_EVENT_KEY_DOWN) {
                    lobby_handle_key(ev.key.key);
                }
            }

            /* ── Title + results events ───────────────────────────────── */
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if ((ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) &&
                    g_state.current == GS_TITLE) {
                    statemachine_request(&g_state, GS_LOBBY);
#ifndef FC_PLATFORM_IOS
                    if (g_lobby.sub == LS_SETUP)
                        SDL_StartTextInput(NULL);
#endif
                }
                if (ev.key.key == SDLK_ESCAPE &&
                    g_state.current == GS_MATCH_SEARCH) {
                    net_client_disconnect();
                    statemachine_request(&g_state, GS_TITLE);
                }
                if ((ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) &&
                    g_state.current == GS_RESULTS) {
                    net_client_disconnect();
                    lobby_init();
                    statemachine_request(&g_state, GS_LOBBY);
#ifndef FC_PLATFORM_IOS
                    if (g_lobby.sub == LS_SETUP)
                        SDL_StartTextInput(NULL);
#endif
                }
            }

            input_handle_event(&ev);
        } /* end SDL_PollEvent */

        net_client_receive();

        /* ── Lobby state machine (per-frame logic) ────────────────────── */
        if (g_state.current == GS_LOBBY) {

            /* LS_CREATING: blocking HTTP POST */
            if (g_lobby.sub == LS_CREATING) {
                lobby_create_room();   /* transitions to LS_SHOW_CODE or LS_ROLE */
            }

            /* NET_READY while in lobby (both P1 and P2): go to loadout */
            if ((g_lobby.sub == LS_SHOW_CODE || g_lobby.sub == LS_WAITING) &&
                net_client_get_state() == NET_READY) {
                g_lobby.sub = LS_LOADOUT;
                g_local_slot = net_client_get_match_info()->your_slot;
                net_client_ack_start();
                SDL_Log("[lobby] Both connected → loadout selection");
            }

            /* Network error in lobby */
            if ((g_lobby.sub == LS_SHOW_CODE || g_lobby.sub == LS_WAITING) &&
                net_client_get_state() == NET_ERROR) {
                SDL_snprintf(g_lobby.error_msg, sizeof(g_lobby.error_msg),
                             "Connection error — check server URL and try again");
            }

            /* LS_LOADOUT → LS_READY: both loadouts determined */
            if (g_lobby.sub == LS_LOADOUT && g_lobby.my_loadout != 0) {
                int opp_loadout;
                if (g_lobby.local_mode) {
                    /* Local play: P1 picked, use LOADOUT_NONE for P2 for simplicity */
                    opp_loadout = LOADOUT_NONE;
                } else {
                    opp_loadout = net_client_get_opponent_loadout();
                }
                if (opp_loadout >= 0) {
                    g_lobby.sub = LS_READY;

                    int p1_lt = g_lobby.is_p1 ? g_lobby.my_loadout : opp_loadout;
                    int p2_lt = g_lobby.is_p1 ? opp_loadout : g_lobby.my_loadout;
                    if (g_lobby.local_mode) {
                        p1_lt = g_lobby.my_loadout;
                        p2_lt = LOADOUT_NONE;
                    }

                    simulation_init(0, 1,
                                    (LoadoutType)p1_lt,
                                    (LoadoutType)p2_lt);
                    statemachine_request(&g_state, GS_FIGHT);
                    SDL_Log("[lobby] Fight! P1 loadout=%d P2 loadout=%d",
                            p1_lt, p2_lt);
                }
            }
        }

        /* ── GS_MATCH_SEARCH (legacy CLI-args path) ───────────────────── */
        if (g_state.current == GS_MATCH_SEARCH) {
            NetState ns = net_client_get_state();
            if (ns == NET_READY) {
                const NetMatchInfo *info = net_client_get_match_info();
                simulation_init(info->p1_fighter, info->p2_fighter,
                                LOADOUT_NONE, LOADOUT_NONE);
                g_local_slot = info->your_slot;
                net_client_ack_start();
                statemachine_request(&g_state, GS_FIGHT);
                SDL_Log("[main] Fight! slot=%d fighters=%d vs %d",
                        g_local_slot, info->p1_fighter, info->p2_fighter);
            } else if (ns == NET_ERROR) {
                SDL_Log("[main] Network error in MATCH_SEARCH");
            }
        }

        /* ── FIGHT → RESULTS ─────────────────────────────────────────── */
        if (g_state.current == GS_FIGHT && net_client_is_network_mode()) {
            NetState ns = net_client_get_state();
            if (ns == NET_ENDED || ns == NET_ERROR)
                statemachine_request(&g_state, GS_RESULTS);
        }

        /* ── Fixed-step simulation ────────────────────────────────────── */
        gameloop_begin_frame(&g_loop, NULL);
        int ticks = 0;
        while (gameloop_should_tick(&g_loop) && ticks < FC_MAX_FRAME_SKIP) {
            statemachine_tick(&g_state);
            input_tick();

            if (g_state.current == GS_FIGHT) {
                uint16_t local  = input_get_local();
                uint16_t remote = rollback_get_remote(g_loop.current_frame);

                if (rollback_needs_resim())
                    rollback_rewind_and_resim(g_loop.current_frame);

                uint16_t p1_in = (g_local_slot == 0) ? local : remote;
                uint16_t p2_in = (g_local_slot == 1) ? local : remote;

                simulation_tick(p1_in, p2_in);
                rollback_save(g_loop.current_frame);
                net_client_send_input(g_loop.current_frame, local);
            }
            ticks++;
        }

        /* ── Render ───────────────────────────────────────────────────── */
        float alpha = gameloop_render_alpha(&g_loop);
        renderer_begin();
        switch (g_state.current) {
            case GS_LOBBY:
                render_lobby(renderer_get());
                break;
            case GS_TITLE:
                render_title(renderer_get());
                break;
            case GS_MATCH_SEARCH:
                render_searching(renderer_get());
                break;
            case GS_FIGHT:
            case GS_TRAINING:
                simulation_render(alpha);
                break;
            case GS_RESULTS:
                render_results(renderer_get());
                break;
            default:
                break;
        }
        renderer_end();

        audio_update();
    }

    /* ── Shutdown ─────────────────────────────────────────────────────── */
    rollback_shutdown();
    net_client_shutdown();
    audio_shutdown();
    input_shutdown();
    renderer_shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
