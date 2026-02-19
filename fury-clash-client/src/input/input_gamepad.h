#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>

void     input_gamepad_init(void);
void     input_gamepad_shutdown(void);
void     input_gamepad_event(const SDL_Event *ev);
uint16_t input_gamepad_read(void);
