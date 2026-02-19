#include "hud.h"
#include "renderer.h"
#include "../core/config.h"
#include <SDL3/SDL.h>

static void draw_health_bar(SDL_Renderer *ren, float x, float y, float w,
                             int hp, int max_hp, int flip) {
    float ratio = (float)hp / (float)max_hp;
    float bar_w = w * ratio;

    /* Background */
    SDL_SetRenderDrawColor(ren, 40, 0, 0, 255);
    SDL_FRect bg = { x, y, w, 18 };
    SDL_RenderFillRect(ren, &bg);

    /* Health fill (left-aligned for P1, right-aligned for P2) */
    SDL_SetRenderDrawColor(ren, 60, 200, 60, 255);
    SDL_FRect bar = flip ? (SDL_FRect){ x + w - bar_w, y, bar_w, 18 }
                         : (SDL_FRect){ x, y, bar_w, 18 };
    SDL_RenderFillRect(ren, &bar);

    /* Border */
    SDL_SetRenderDrawColor(ren, 200, 200, 200, 200);
    SDL_RenderRect(ren, &bg);
}

static void draw_super_meter(SDL_Renderer *ren, float x, float y, float w, float meter) {
    SDL_SetRenderDrawColor(ren, 20, 20, 60, 255);
    SDL_FRect bg = { x, y, w, 8 };
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawColor(ren, 80, 120, 255, 255);
    SDL_FRect bar = { x, y, w * meter, 8 };
    SDL_RenderFillRect(ren, &bar);
}

void hud_render(const Fighter *p1, const Fighter *p2, const RoundState *rs) {
    SDL_Renderer *ren = renderer_get();
    float margin = 20.0f;
    float bar_w  = FC_GAME_W / 2.0f - margin * 2.0f - 30.0f;

    /* P1 health bar (left side) */
    draw_health_bar(ren, margin, margin, bar_w, p1->health, p1->max_health, 0);
    /* P1 super meter */
    draw_super_meter(ren, margin, margin + 22, bar_w, p1->super_meter);

    /* P2 health bar (right side, right-aligned) */
    float p2x = FC_GAME_W - margin - bar_w;
    draw_health_bar(ren, p2x, margin, bar_w, p2->health, p2->max_health, 1);
    draw_super_meter(ren, p2x, margin + 22, bar_w, p2->super_meter);

    /* Timer (center) */
    int sec = rs->frame_timer / FC_TICK_RATE;
    char buf[8];
    SDL_snprintf(buf, sizeof(buf), "%02d", sec);
    /* TODO: draw bitmap font */
    (void)buf;
}
