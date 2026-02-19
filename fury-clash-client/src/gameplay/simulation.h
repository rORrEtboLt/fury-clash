#pragma once
#include <stdint.h>
#include "fighter.h"
#include "round.h"

/* The complete serialisable game state — must be < 512 bytes */
typedef struct SimState {
    Fighter    fighters[2];
    RoundState round;
    uint32_t   frame;
} SimState;

void simulation_init(int p1_fighter_id, int p2_fighter_id);
void simulation_tick(uint16_t p1_input, uint16_t p2_input);
void simulation_render(float alpha);

/* Snapshot for rollback */
void simulation_save(SimState *out);
void simulation_restore(const SimState *in);

/* Expose fighters for rollback read */
const Fighter *simulation_get_fighter(int slot);
