#include "input_keyboard.h"
#include "../network/protocol.h"
#include <string.h>

static uint16_t g_state = 0;

static void set_bit(SDL_Keycode key, int pressed) {
    uint16_t bit = 0;
    switch (key) {
        case SDLK_UP:    case SDLK_W: bit = INPUT_UP;          break;
        case SDLK_DOWN:  case SDLK_S: bit = INPUT_DOWN;        break;
        case SDLK_LEFT:  case SDLK_A: bit = INPUT_LEFT;        break;
        case SDLK_RIGHT: case SDLK_D: bit = INPUT_RIGHT;       break;
        case SDLK_U:                  bit = INPUT_LIGHT_PUNCH;  break;
        case SDLK_I:                  bit = INPUT_HEAVY_PUNCH;  break;
        case SDLK_J:                  bit = INPUT_LIGHT_KICK;   break;
        case SDLK_K:                  bit = INPUT_HEAVY_KICK;   break;
        case SDLK_O:                  bit = INPUT_SPECIAL1;     break;
        case SDLK_P:                  bit = INPUT_SPECIAL2;     break;
        case SDLK_L:                  bit = INPUT_SUPER;        break;
        case SDLK_H:                  bit = INPUT_GRAB;         break;
        default: return;
    }
    if (pressed) g_state |= bit;
    else         g_state &= ~bit;
}

void input_keyboard_init(void)    { g_state = 0; }
void input_keyboard_shutdown(void){ g_state = 0; }

void input_keyboard_event(const SDL_Event *ev) {
    if (ev->type == SDL_EVENT_KEY_DOWN) set_bit(ev->key.key, 1);
    if (ev->type == SDL_EVENT_KEY_UP)   set_bit(ev->key.key, 0);
}

uint16_t input_keyboard_read(void) { return g_state; }
