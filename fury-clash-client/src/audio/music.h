#pragma once
#include <SDL3/SDL.h>
void music_init(SDL_AudioDeviceID dev);
void music_shutdown(void);
void music_play(int id, int loop);
void music_stop(void);
void music_set_volume(float v);
void music_update(void);
