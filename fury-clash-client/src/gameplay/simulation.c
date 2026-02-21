#include "simulation.h"
#include "hitbox.h"
#include "physics.h"
#include "../render/renderer.h"
#include "../render/hud.h"
#include "../core/config.h"
#include <SDL3/SDL.h>
#include <string.h>

static SimState g_sim;

void simulation_init(int p1_fighter_id, int p2_fighter_id,
                     LoadoutType p1_loadout, LoadoutType p2_loadout)
{
    memset(&g_sim, 0, sizeof(g_sim));
    fighter_init(&g_sim.fighters[0], p1_fighter_id, 0);
    fighter_init(&g_sim.fighters[1], p2_fighter_id, 1);
    round_init(&g_sim.round);

    /* Apply pre-match loadout modifiers */
    for (int i = 0; i < 2; i++) {
        Fighter *f = &g_sim.fighters[i];
        LoadoutType lt = (i == 0) ? p1_loadout : p2_loadout;
        f->loadout     = lt;
        f->damage_pct  = 100;   /* default: ×1.0 */
        if (lt == LOADOUT_FIRE) {
            f->damage_pct = 120;   /* fire pack: +20% damage dealt */
        } else if (lt == LOADOUT_FROST) {
            f->health     += 20;   /* frost pack: +20 HP */
            f->max_health += 20;
        }
    }

    SDL_Log("[sim] init fighters %d vs %d  loadouts %d/%d",
            p1_fighter_id, p2_fighter_id, (int)p1_loadout, (int)p2_loadout);
}

void simulation_tick(uint16_t p1_input, uint16_t p2_input) {
    if (g_sim.round.match_over) return;

    Fighter *p1 = &g_sim.fighters[0];
    Fighter *p2 = &g_sim.fighters[1];

    fighter_tick(p1, p1_input, p2);
    fighter_tick(p2, p2_input, p1);

    /* Hitbox resolution (both directions) */
    if (hitbox_check(p1, p2)) hitbox_resolve(p1, p2);
    if (hitbox_check(p2, p1)) hitbox_resolve(p2, p1);

    /* Push-out */
    physics_push_apart(p1, p2);

    /* Round logic */
    round_tick(&g_sim.round, p1->health, p2->health);

    g_sim.frame++;
}

void simulation_render(float alpha) {
    (void)alpha;
    SDL_Renderer *ren = renderer_get();

    /* Stage floor */
    float lx, ly, rx, ry;
    renderer_world_to_screen(FC_STAGE_LEFT,  FC_GROUND_Y, &lx, &ly);
    renderer_world_to_screen(FC_STAGE_RIGHT, FC_GROUND_Y, &rx, &ry);
    SDL_SetRenderDrawColor(ren, 60, 50, 40, 255);
    SDL_FRect floor_rect = { lx, ly, rx - lx, 8.0f };
    SDL_RenderFillRect(ren, &floor_rect);

    /* Fighter placeholder rectangles */
    static const Uint8 pal[2][3] = { {80, 140, 255}, {255, 80, 80} };
    for (int i = 0; i < 2; i++) {
        const Fighter *f = &g_sim.fighters[i];
        float sx, sy, ex, ey;
        renderer_world_to_screen(f->x + f->hurtbox.x,
                                  f->y + f->hurtbox.y, &sx, &sy);
        renderer_world_to_screen(f->x + f->hurtbox.x + f->hurtbox.w,
                                  f->y + f->hurtbox.y + f->hurtbox.h, &ex, &ey);
        SDL_SetRenderDrawColor(ren, pal[i][0], pal[i][1], pal[i][2], 200);
        SDL_FRect rect = { sx, sy, ex - sx, ey - sy };
        SDL_RenderFillRect(ren, &rect);
        SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
        SDL_RenderRect(ren, &rect);
    }

    /* HUD */
    hud_render(&g_sim.fighters[0], &g_sim.fighters[1], &g_sim.round);
}

void simulation_save(SimState *out) {
    *out = g_sim;
}

void simulation_restore(const SimState *in) {
    g_sim = *in;
}

const Fighter *simulation_get_fighter(int slot) {
    if (slot < 0 || slot > 1) return NULL;
    return &g_sim.fighters[slot];
}
