#include "renderer.h"
#include "../core/config.h"

static SDL_Renderer *g_ren = NULL;

/* Camera state */
static float g_cam_x = FC_GAME_W / 2.0f;
static float g_cam_y = FC_GAME_H / 2.0f;
static float g_cam_zoom = 1.0f;

int renderer_init(SDL_Window *window, const PlatformInfo *platform) {
    (void)platform;
    g_ren = SDL_CreateRenderer(window, NULL);
    if (!g_ren) return 0;
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(g_ren, 1);
    SDL_SetRenderLogicalPresentation(g_ren, FC_GAME_W, FC_GAME_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return 1;
}

void renderer_shutdown(void) {
    if (g_ren) { SDL_DestroyRenderer(g_ren); g_ren = NULL; }
}

void renderer_begin(void) {
    SDL_SetRenderDrawColor(g_ren, 10, 10, 18, 255);
    SDL_RenderClear(g_ren);
}

void renderer_end(void) {
    SDL_RenderPresent(g_ren);
}

SDL_Renderer *renderer_get(void) { return g_ren; }

void renderer_set_camera(float cx, float cy, float zoom) {
    g_cam_x = cx; g_cam_y = cy; g_cam_zoom = zoom;
}

void renderer_world_to_screen(float wx, float wy, float *sx, float *sy) {
    float hw = FC_GAME_W / 2.0f;
    float hh = FC_GAME_H / 2.0f;
    *sx = (wx - g_cam_x) * g_cam_zoom + hw;
    *sy = (wy - g_cam_y) * g_cam_zoom + hh;
}
