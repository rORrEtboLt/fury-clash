#include "input_touch.h"
#include "../network/protocol.h"
#include <string.h>
#include <math.h>

static TouchLayout g_layout;
static uint16_t    g_state = 0;

/* Joystick tracking */
static SDL_FingerID g_joy_finger = 0;
static float        g_joy_ax = 0, g_joy_ay = 0;   /* anchor point */
static float        g_joy_cx = 0, g_joy_cy = 0;   /* current point */
static int          g_joy_active = 0;

void input_touch_init(void)     { memset(&g_layout, 0, sizeof(g_layout)); }
void input_touch_shutdown(void) {}

void input_touch_layout(float w, float h, float dpi, int is_tablet) {
    g_layout.screen_w     = w;
    g_layout.screen_h     = h;
    g_layout.dpi_scale    = dpi;
    g_layout.is_tablet    = is_tablet;
    g_layout.button_radius = dpi > 2.0f ? 38.0f : 32.0f;
    if (is_tablet) g_layout.button_radius *= 1.2f;
    g_layout.joystick_radius = g_layout.button_radius * 2.5f;

    float aspect = 16.0f / 9.0f;
    float vw = w, vh = w / aspect;
    if (vh > h * 0.65f) { vh = h * 0.65f; vw = vh * aspect; }
    g_layout.viewport = (SDL_FRect){ (w - vw) / 2, 0, vw, vh };

    float ctrl_h = h - vh;
    g_layout.joystick_zone = (SDL_FRect){ 0, vh, w * 0.4f, ctrl_h };
    g_layout.button_zone   = (SDL_FRect){ w * 0.6f, vh, w * 0.4f, ctrl_h };
}

static void process_joystick(float cx, float cy) {
    float dx = cx - g_joy_ax;
    float dy = cy - g_joy_ay;
    float len = sqrtf(dx * dx + dy * dy);
    float dead = 10.0f;

    g_state &= ~(INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT);
    if (len > dead) {
        float ndx = dx / len, ndy = dy / len;
        if (ndy < -0.5f) g_state |= INPUT_UP;
        if (ndy >  0.5f) g_state |= INPUT_DOWN;
        if (ndx < -0.5f) g_state |= INPUT_LEFT;
        if (ndx >  0.5f) g_state |= INPUT_RIGHT;
    }
}

void input_touch_event(const SDL_Event *ev) {
    if (ev->type == SDL_EVENT_FINGER_DOWN) {
        float fx = ev->tfinger.x * g_layout.screen_w;
        float fy = ev->tfinger.y * g_layout.screen_h;
        /* Check if in joystick zone */
        SDL_FRect *jz = &g_layout.joystick_zone;
        if (!g_joy_active && fx >= jz->x && fx <= jz->x + jz->w &&
                              fy >= jz->y && fy <= jz->y + jz->h) {
            g_joy_finger = ev->tfinger.fingerID;
            g_joy_ax = fx; g_joy_ay = fy;
            g_joy_active = 1;
        }
    }
    if (ev->type == SDL_EVENT_FINGER_MOTION && g_joy_active &&
        ev->tfinger.fingerID == g_joy_finger) {
        process_joystick(ev->tfinger.x * g_layout.screen_w,
                         ev->tfinger.y * g_layout.screen_h);
    }
    if (ev->type == SDL_EVENT_FINGER_UP && g_joy_active &&
        ev->tfinger.fingerID == g_joy_finger) {
        g_joy_active = 0;
        g_state &= ~(INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT);
    }
}

uint16_t input_touch_read(void) { return g_state; }

const TouchLayout *input_touch_get_layout(void) { return &g_layout; }
