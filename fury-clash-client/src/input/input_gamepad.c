#include "input_gamepad.h"
#include "../network/protocol.h"
#include <string.h>

static SDL_Gamepad *g_pad = NULL;
static uint16_t    g_state = 0;

void input_gamepad_init(void) {
    /* Open first available gamepad */
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids && count > 0)
        g_pad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
}

void input_gamepad_shutdown(void) {
    if (g_pad) { SDL_CloseGamepad(g_pad); g_pad = NULL; }
}

void input_gamepad_event(const SDL_Event *ev) {
    if (ev->type == SDL_EVENT_GAMEPAD_ADDED && !g_pad)
        g_pad = SDL_OpenGamepad(ev->gdevice.which);
    if (ev->type == SDL_EVENT_GAMEPAD_REMOVED && g_pad)
        if (SDL_GetGamepadID(g_pad) == ev->gdevice.which) {
            SDL_CloseGamepad(g_pad);
            g_pad = NULL;
        }
}

uint16_t input_gamepad_read(void) {
    if (!g_pad) return 0;
    g_state = 0;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_DPAD_UP))    g_state |= INPUT_UP;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  g_state |= INPUT_DOWN;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  g_state |= INPUT_LEFT;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) g_state |= INPUT_RIGHT;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_SOUTH))      g_state |= INPUT_LIGHT_PUNCH;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_EAST))       g_state |= INPUT_HEAVY_PUNCH;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_WEST))       g_state |= INPUT_LIGHT_KICK;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_NORTH))      g_state |= INPUT_HEAVY_KICK;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) g_state |= INPUT_SPECIAL1;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))  g_state |= INPUT_SPECIAL2;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_START))      g_state |= INPUT_SUPER;
    if (SDL_GetGamepadButton(g_pad, SDL_GAMEPAD_BUTTON_BACK))       g_state |= INPUT_GRAB;
    return g_state;
}
