#pragma once

/* ── Timing ─────────────────────────────────────────────────────────── */
#define FC_TICK_RATE        60
#define FC_TICK_DT          (1.0 / FC_TICK_RATE)
#define FC_MAX_FRAME_SKIP   5       /* max ticks per host frame to avoid spiral */

/* ── Display ────────────────────────────────────────────────────────── */
#define FC_GAME_W           1280
#define FC_GAME_H           720
#define FC_GAME_ASPECT      (16.0f / 9.0f)

/* ── Rollback ───────────────────────────────────────────────────────── */
#define FC_ROLLBACK_FRAMES  128     /* ring buffer depth */
#define FC_MAX_ROLLBACK     8       /* frames before warning */
#define FC_INPUT_DELAY      2       /* frames of forced input delay */

/* ── Network ────────────────────────────────────────────────────────── */
#define FC_WS_RECONNECT_MS  3000
#define FC_PING_INTERVAL_MS 1000
#define FC_INPUT_SEND_HZ    60      /* send once per sim tick */

/* iOS: default relay server URL (set to your Linux machine's LAN IP).
 * Override at build time: cmake -DFC_IOS_SERVER_URL="ws://192.168.1.50:3002"
 * The iOS connect-setup screen pre-fills this but lets the user edit it. */
#ifndef FC_IOS_SERVER_URL
#define FC_IOS_SERVER_URL   "ws://192.168.1.100:3002"
#endif

/* ── Fighters ───────────────────────────────────────────────────────── */
#define FC_MAX_FIGHTERS     2
#define FC_MAX_HITBOXES     4
#define FC_MAX_HEALTH       1000
#define FC_ROUNDS_TO_WIN    2
#define FC_ROUND_TIME_SEC   99

/* ── Physics ────────────────────────────────────────────────────────── */
#define FC_GRAVITY          1800.0f /* px / s^2 */
#define FC_GROUND_Y         540.0f  /* world-space floor */
#define FC_STAGE_LEFT       50.0f
#define FC_STAGE_RIGHT      1230.0f

/* ── Camera ─────────────────────────────────────────────────────────── */
#define FC_CAM_ZOOM_MIN     0.7f
#define FC_CAM_ZOOM_MAX     1.0f
#define FC_CAM_LERP         0.10f
#define FC_CAM_ZOOM_LERP    0.08f

/* ── Audio ──────────────────────────────────────────────────────────── */
#define FC_AUDIO_SAMPLE_RATE    44100
#define FC_AUDIO_CHANNELS       2
#define FC_MAX_SFX_CHANNELS     32
