#include "fighter_data.h"
#include <SDL3/SDL.h>
#include <string.h>

/* Hardcoded fallback data until JSON loader is wired up.
 * Move index layout (shared by all fighters for now):
 *   0  stand_LP   1  stand_HP   2  qcf_LP
 *   3  jump_LP    4  jump_HP        ← air attacks (JUMP_MOVE_FIRST = 3)
 */
static MoveData ryu_moves[] = {
    {
        "stand_LP", 16, 4, 3, 9,
        { 40, 8, 2, 12, 0.5f, 0.0f, 0 },
        { 10, -80, 50, 20 }
    },
    {
        "stand_HP", 28, 8, 4, 16,
        { 80, 8, 4, 18, 3.5f, 0.0f, 0 },
        { 20, -100, 60, 30 }
    },
    {
        "qcf_LP", 35, 12, 8, 15,
        { 100, 10, 6, 20, 4.0f, -2.0f, 0 },
        { 30, -90, 80, 30 }
    },
    /* ── Air attacks ─────────────────────────────────────────────────
     * Hitbox x is forward of body center (flipped by facing in hitbox.c).
     * Hitbox y range reaches down to hit grounded opponents on descent.
     * total = startup + active + recovery
     */
    {
        "jump_LP", 22, 6, 6, 10,
        { 50, 8, 3, 12, 1.5f, 0.0f, 0 },
        { 5, -80, 60, 50 }   /* x:5–65 fwd, y:−80 to −30 (mid-low) */
    },
    {
        "jump_HP", 32, 9, 5, 18,
        { 90, 10, 4, 18, 3.5f, 0.0f, 0 },
        { 10, -90, 65, 60 }  /* x:10–75 fwd, y:−90 to −30 (wider) */
    },
};

static FighterData fighters[] = {
    { "ryu", 200, 160, -900.0f, 180.0f, ryu_moves, 5 },
    { "ken", 220, 170, -920.0f, 200.0f, ryu_moves, 5 }, /* placeholder */
};

#define NUM_FIGHTERS 2

const FighterData *fighter_data_get(int fighter_id) {
    if (fighter_id < 0 || fighter_id >= NUM_FIGHTERS) return NULL;
    return &fighters[fighter_id];
}

int fighter_data_load_all(void) {
    /* TODO: parse assets/fighters/<name>/data.json and populate fighters[] */
    SDL_Log("[fighter_data] loaded %d fighters (hardcoded)", NUM_FIGHTERS);
    return 1;
}
