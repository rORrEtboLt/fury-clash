#include "input.h"
#include "input_keyboard.h"
#include "input_gamepad.h"
#include "input_touch.h"
#include "../network/protocol.h"
#include <string.h>

static uint16_t g_current = 0;

void input_init(void) {
    input_keyboard_init();
    input_gamepad_init();
    input_touch_init();
}

void input_shutdown(void) {
    input_keyboard_shutdown();
    input_gamepad_shutdown();
    input_touch_shutdown();
}

void input_handle_event(const SDL_Event *ev) {
    input_keyboard_event(ev);
    input_gamepad_event(ev);
    input_touch_event(ev);
}

void input_tick(void) {
    uint16_t kb  = input_keyboard_read();
    uint16_t gp  = input_gamepad_read();
    uint16_t tch = input_touch_read();
    g_current = kb | gp | tch;
}

uint16_t input_get_local(void) {
    return g_current;
}

void input_touch_layout_update(float screen_w, float screen_h,
                                float dpi_scale, int is_tablet) {
    input_touch_layout(screen_w, screen_h, dpi_scale, is_tablet);
}
