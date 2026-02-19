#pragma once
#include <SDL3/SDL.h>
void sfx_init(SDL_AudioDeviceID dev);
void sfx_shutdown(void);
void sfx_play(int id);
void sfx_stop(void);
void sfx_set_volume(float v);
void sfx_update(void);
