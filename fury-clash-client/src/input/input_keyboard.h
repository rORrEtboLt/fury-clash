#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>

void     input_keyboard_init(void);
void     input_keyboard_shutdown(void);
void     input_keyboard_event(const SDL_Event *ev);
uint16_t input_keyboard_read(void);
