/*
 * Fury Clash — main.c
 * Entry point: initialise SDL3, hand off to game loop.
 *
 * Desktop usage:
 *   ./fury-clash                                        — local single-player
 *   ./fury-clash ws://HOST:PORT ROOM_ID PLAYER_ID SLOT  — network mode
 *
 * iOS usage (no command-line args):
 *   A "Connect Setup" screen is shown first.  The user types the server URL
 *   and Room ID using the on-screen keyboard, then taps "P1" or "P2" to join.
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
#include "audio/audio.h"
#include "gameplay/simulation.h"
#include "network/rollback.h"

/* ── Globals ────────────────────────────────────────────────────────────── */
static PlatformInfo g_platform;
static StateMachine g_state;
static GameLoop     g_loop;
static int          g_local_slot = 0;

/* ── iOS connect-setup state ────────────────────────────────────────────── */
#ifdef FC_PLATFORM_IOS

typedef enum ConnectField {
    CF_SERVER = 0,
    CF_ROOM   = 1,
} ConnectField;

static struct {
    char         server_url[128];   /* pre-filled from FC_IOS_SERVER_URL */
    char         room_id[64];
    ConnectField active_field;
    int          ready;             /* 1 when user hits "Join P1/P2" */
    int          chosen_slot;       /* 0 or 1 */
    int          pending_text_input; /* deferred SDL_StartTextInput (stop+start must span frames) */
} g_ios_cfg;

static void ios_cfg_init(void)
{
    memset(&g_ios_cfg, 0, sizeof(g_ios_cfg));
    SDL_snprintf(g_ios_cfg.server_url, sizeof(g_ios_cfg.server_url),
                 "%s", FC_IOS_SERVER_URL);
    g_ios_cfg.active_field = CF_SERVER;
}

static void ios_cfg_append(char *buf, int maxlen, const char *text)
{
    int n = (int)strlen(buf);
    int tlen = (int)strlen(text);
    if (n + tlen < maxlen - 1) {
        memcpy(buf + n, text, (size_t)tlen);
        buf[n + tlen] = '\0';
    }
}

static void ios_cfg_backspace(char *buf)
{
    int n = (int)strlen(buf);
    if (n > 0) buf[n - 1] = '\0';
}

#endif /* FC_PLATFORM_IOS */

/* ── Render helpers ─────────────────────────────────────────────────────── */

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
        const char *prompt = net_client_is_network_mode()
            ? "Press ENTER to connect"
            : "Press ENTER to play";
        SDL_RenderDebugTextFormat(ren, 450.0f, 420.0f, "%s", prompt);
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

/* ── iOS connect-setup screen ───────────────────────────────────────────── */
#ifdef FC_PLATFORM_IOS

static void render_ios_setup(SDL_Renderer *ren)
{
    /* Background */
    SDL_SetRenderDrawColor(ren, 15, 15, 40, 255);
    SDL_FRect full = {0, 0, FC_GAME_W, FC_GAME_H};
    SDL_RenderFillRect(ren, &full);

    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 530.0f, 60.0f, "FURY CLASH");
    SDL_RenderDebugTextFormat(ren, 450.0f, 90.0f, "Network Setup");

    /* Server URL field */
    int srv_active = (g_ios_cfg.active_field == CF_SERVER);
    SDL_SetRenderDrawColor(ren, srv_active ? 100 : 50, srv_active ? 150 : 80, 255, 255);
    SDL_FRect srv_box = {160.0f, 180.0f, 960.0f, 60.0f};
    SDL_RenderFillRect(ren, &srv_box);
    SDL_SetRenderDrawColor(ren, 200, 200, 255, 255);
    SDL_RenderRect(ren, &srv_box);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 170.0f, 188.0f, "Server URL:");
    SDL_RenderDebugTextFormat(ren, 170.0f, 210.0f, "%s%s",
        g_ios_cfg.server_url,
        (srv_active && (SDL_GetTicks() / 500) % 2) ? "|" : "");

    /* Room ID field */
    int rm_active = (g_ios_cfg.active_field == CF_ROOM);
    SDL_SetRenderDrawColor(ren, rm_active ? 100 : 50, rm_active ? 150 : 80, 255, 255);
    SDL_FRect rm_box = {160.0f, 280.0f, 960.0f, 60.0f};
    SDL_RenderFillRect(ren, &rm_box);
    SDL_SetRenderDrawColor(ren, 200, 200, 255, 255);
    SDL_RenderRect(ren, &rm_box);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 170.0f, 288.0f, "Room ID:");
    SDL_RenderDebugTextFormat(ren, 170.0f, 310.0f, "%s%s",
        g_ios_cfg.room_id,
        (rm_active && (SDL_GetTicks() / 500) % 2) ? "|" : "");

    SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
    SDL_RenderDebugTextFormat(ren, 160.0f, 360.0f,
        "Tap a field to edit. Get Room ID from create-room.sh on your PC.");

    /* Slot buttons */
    SDL_SetRenderDrawColor(ren, 50, 120, 220, 255);
    SDL_FRect p1btn = {230.0f, 420.0f, 280.0f, 80.0f};
    SDL_RenderFillRect(ren, &p1btn);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 315.0f, 452.0f, "Join as P1");

    SDL_SetRenderDrawColor(ren, 220, 60, 60, 255);
    SDL_FRect p2btn = {770.0f, 420.0f, 280.0f, 80.0f};
    SDL_RenderFillRect(ren, &p2btn);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDebugTextFormat(ren, 855.0f, 452.0f, "Join as P2");
}

static void ios_setup_handle_touch(float tx, float ty)
{
    /* Field tap regions — stop+start in the same runloop tick causes iOS to not
     * show the keyboard.  We stop here and set a flag; SDL_StartTextInput is
     * called at the top of the next frame so the two calls span separate ticks. */
    if (tx >= 160 && tx <= 1120) {
        if (ty >= 180 && ty <= 240) {
            g_ios_cfg.active_field = CF_SERVER;
            SDL_StopTextInput(NULL);
            g_ios_cfg.pending_text_input = 1;
            return;
        }
        if (ty >= 280 && ty <= 340) {
            g_ios_cfg.active_field = CF_ROOM;
            SDL_StopTextInput(NULL);
            g_ios_cfg.pending_text_input = 1;
            return;
        }
    }
    /* Join P1 button */
    if (tx >= 230 && tx <= 510 && ty >= 420 && ty <= 500) {
        if (g_ios_cfg.room_id[0] != '\0') {
            SDL_StopTextInput(NULL);
            g_ios_cfg.chosen_slot = 0;
            g_ios_cfg.ready       = 1;
        }
    }
    /* Join P2 button */
    if (tx >= 770 && tx <= 1050 && ty >= 420 && ty <= 500) {
        if (g_ios_cfg.room_id[0] != '\0') {
            SDL_StopTextInput(NULL);
            g_ios_cfg.chosen_slot = 1;
            g_ios_cfg.ready       = 1;
        }
    }
}

#endif /* FC_PLATFORM_IOS */

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* ── Parse arguments (desktop/network mode) ──────────────────────── */
#ifdef FC_PLATFORM_IOS
    (void)argc; (void)argv;
    ios_cfg_init();
#else
    if (argc == 5) {
        const char *ws_url    = argv[1];
        const char *room_id   = argv[2];
        const char *player_id = argv[3];
        int         slot      = atoi(argv[4]);
        if (slot != 0 && slot != 1) { SDL_Log("SLOT must be 0 or 1"); return 1; }
        net_client_configure(ws_url, room_id, player_id, slot);
        g_local_slot = slot;
    } else if (argc != 1) {
        SDL_Log("Usage: %s [ws://HOST:PORT ROOM_ID PLAYER_ID SLOT]", argv[0]);
        return 1;
    }
#endif

    /* ── SDL3 init ───────────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    platform_init(&g_platform);

    SDL_Window *window = SDL_CreateWindow(
        "Fury Clash",
        FC_GAME_W, FC_GAME_H,
#ifdef FC_PLATFORM_IOS
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
#else
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

    /* ── Initial state ───────────────────────────────────────────────── */
#ifdef FC_PLATFORM_IOS
    /* iOS always starts on the connect-setup screen */
    statemachine_request(&g_state, GS_CHAR_SELECT);  /* repurposed as iOS setup */
#else
    if (net_client_is_network_mode()) {
        net_client_connect(NULL);
        statemachine_request(&g_state, GS_MATCH_SEARCH);
    }
#endif

    /* ── Main loop ──────────────────────────────────────────────────── */
    while (g_loop.running) {

#ifdef FC_PLATFORM_IOS
        /* Deferred keyboard activation: SDL_StopTextInput was called on the
         * previous frame when the user tapped a field.  Starting here (a new
         * tick) ensures stop and start never occur in the same runloop cycle,
         * which would prevent iOS from showing the keyboard. */
        if (g_ios_cfg.pending_text_input && g_state.current == GS_CHAR_SELECT) {
            g_ios_cfg.pending_text_input = 0;
            SDL_StartTextInput(NULL);
        }
#endif

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT)
                g_loop.running = 0;

#ifdef FC_PLATFORM_IOS
            /* ── iOS: connect-setup screen events ─────────────────── */
            if (g_state.current == GS_CHAR_SELECT) {

                if (ev.type == SDL_EVENT_FINGER_DOWN) {
                    /* Convert normalised finger coords → window px → logical game coords */
                    int ww, wh;
                    SDL_GetWindowSize(window, &ww, &wh);
                    float tx, ty;
                    SDL_RenderCoordinatesFromWindow(renderer_get(),
                        ev.tfinger.x * (float)ww,
                        ev.tfinger.y * (float)wh,
                        &tx, &ty);
                    ios_setup_handle_touch(tx, ty);
                }

                if (ev.type == SDL_EVENT_TEXT_INPUT) {
                    char *dst = (g_ios_cfg.active_field == CF_SERVER)
                                ? g_ios_cfg.server_url : g_ios_cfg.room_id;
                    int maxlen = (g_ios_cfg.active_field == CF_SERVER)
                                ? (int)sizeof(g_ios_cfg.server_url)
                                : (int)sizeof(g_ios_cfg.room_id);
                    ios_cfg_append(dst, maxlen, ev.text.text);
                }

                if (ev.type == SDL_EVENT_KEY_DOWN) {
                    char *dst = (g_ios_cfg.active_field == CF_SERVER)
                                ? g_ios_cfg.server_url : g_ios_cfg.room_id;
                    if (ev.key.key == SDLK_BACKSPACE)   ios_cfg_backspace(dst);
                    if (ev.key.key == SDLK_TAB)
                        g_ios_cfg.active_field =
                            (g_ios_cfg.active_field == CF_SERVER) ? CF_ROOM : CF_SERVER;
                    if (ev.key.key == SDLK_RETURN && g_ios_cfg.room_id[0] != '\0') {
                        SDL_StopTextInput(NULL);
                        g_ios_cfg.chosen_slot = 0;
                        g_ios_cfg.ready       = 1;
                    }
                }
            }
#else
            /* ── Desktop: title / match-search key handling ───────── */
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if ((ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) &&
                    g_state.current == GS_TITLE) {
                    if (net_client_is_network_mode()) {
                        net_client_connect(NULL);
                        statemachine_request(&g_state, GS_MATCH_SEARCH);
                    } else {
                        simulation_init(0, 1);
                        statemachine_request(&g_state, GS_FIGHT);
                    }
                }
                if (ev.key.key == SDLK_ESCAPE &&
                    g_state.current == GS_MATCH_SEARCH) {
                    net_client_disconnect();
                    statemachine_request(&g_state, GS_TITLE);
                }
                if ((ev.key.key == SDLK_RETURN || ev.key.key == SDLK_SPACE) &&
                    g_state.current == GS_RESULTS) {
                    statemachine_request(&g_state, GS_TITLE);
                }
            }
#endif
            input_handle_event(&ev);
        } /* end SDL_PollEvent */

        /* ── iOS: connect-setup → match-search transition ─────────── */
#ifdef FC_PLATFORM_IOS
        if (g_state.current == GS_CHAR_SELECT && g_ios_cfg.ready) {
            g_ios_cfg.ready  = 0;
            g_local_slot     = g_ios_cfg.chosen_slot;

            /* Generate a simple player ID from slot + random suffix */
            char player_id[32];
            SDL_snprintf(player_id, sizeof(player_id),
                         "mobile-p%d-%04x",
                         g_local_slot + 1,
                         (unsigned)(SDL_GetTicks() & 0xFFFF));

            net_client_configure(g_ios_cfg.server_url,
                                 g_ios_cfg.room_id,
                                 player_id,
                                 g_local_slot);
            net_client_connect(NULL);
            statemachine_request(&g_state, GS_MATCH_SEARCH);
        }
#endif

        net_client_receive();

        /* ── MATCH_SEARCH → FIGHT ────────────────────────────────── */
        if (g_state.current == GS_MATCH_SEARCH) {
            NetState ns = net_client_get_state();
            if (ns == NET_READY) {
                const NetMatchInfo *info = net_client_get_match_info();
                simulation_init(info->p1_fighter, info->p2_fighter);
                g_local_slot = info->your_slot;
                net_client_ack_start();
                statemachine_request(&g_state, GS_FIGHT);
                SDL_Log("[main] Fight started! slot=%d fighters=%d vs %d",
                        g_local_slot, info->p1_fighter, info->p2_fighter);
            } else if (ns == NET_ERROR) {
                SDL_Log("[main] Network error in MATCH_SEARCH");
                /* Stay so render_searching shows the error; ESC returns to title */
            }
        }

        /* ── FIGHT → RESULTS ─────────────────────────────────────── */
        if (g_state.current == GS_FIGHT && net_client_is_network_mode()) {
            NetState ns = net_client_get_state();
            if (ns == NET_ENDED || ns == NET_ERROR)
                statemachine_request(&g_state, GS_RESULTS);
        }

        /* ── Fixed-step simulation ──────────────────────────────── */
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

        /* ── Render ─────────────────────────────────────────────── */
        float alpha = gameloop_render_alpha(&g_loop);
        renderer_begin();
        switch (g_state.current) {
#ifdef FC_PLATFORM_IOS
            case GS_CHAR_SELECT:
                render_ios_setup(renderer_get());
                break;
#endif
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

    /* ── Shutdown ───────────────────────────────────────────────────── */
    rollback_shutdown();
    net_client_shutdown();
    audio_shutdown();
    input_shutdown();
    renderer_shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
