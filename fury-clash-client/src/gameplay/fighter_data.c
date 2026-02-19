#include "fighter_data.h"
#include <SDL3/SDL.h>
#include <string.h>

/* Hardcoded fallback data until JSON loader is wired up */
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
};

static FighterData fighters[] = {
    { "ryu", 200, 160, -900.0f, 180.0f, ryu_moves, 3 },
    { "ken", 220, 170, -920.0f, 200.0f, ryu_moves, 3 }, /* placeholder */
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
