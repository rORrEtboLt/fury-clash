#pragma once
#include <SDL3/SDL.h>
#include "../platform/platform.h"

int  renderer_init(SDL_Window *window, const PlatformInfo *platform);
void renderer_shutdown(void);
void renderer_begin(void);
void renderer_end(void);   /* SDL_RenderPresent */

SDL_Renderer *renderer_get(void);

/* Viewport transform helpers */
void renderer_world_to_screen(float wx, float wy, float *sx, float *sy);
void renderer_set_camera(float cx, float cy, float zoom);
