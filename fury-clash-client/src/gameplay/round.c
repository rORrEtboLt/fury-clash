#include "round.h"
#include "../core/config.h"
#include <string.h>

void round_init(RoundState *rs) {
    memset(rs, 0, sizeof(*rs));
    rs->frame_timer = FC_ROUND_TIME_SEC * FC_TICK_RATE;
    rs->round_number = 1;
    rs->winner_slot  = -1;
}

void round_tick(RoundState *rs, int p1_hp, int p2_hp) {
    if (rs->round_over) return;

    rs->frame_timer--;

    /* KO check */
    int p1_ko = (p1_hp <= 0);
    int p2_ko = (p2_hp <= 0);

    if (p1_ko || p2_ko || rs->frame_timer <= 0) {
        rs->round_over = 1;
        if (p1_ko && p2_ko) {
            rs->winner_slot = -1; /* double KO — both get a round */
            rs->p1_rounds_won++;
            rs->p2_rounds_won++;
        } else if (p2_ko || p1_hp > p2_hp) {
            rs->winner_slot = 0;
            rs->p1_rounds_won++;
        } else {
            rs->winner_slot = 1;
            rs->p2_rounds_won++;
        }

        if (rs->p1_rounds_won >= FC_ROUNDS_TO_WIN || rs->p2_rounds_won >= FC_ROUNDS_TO_WIN)
            rs->match_over = 1;
    }
}

void round_start_next(RoundState *rs) {
    rs->round_over   = 0;
    rs->winner_slot  = -1;
    rs->frame_timer  = FC_ROUND_TIME_SEC * FC_TICK_RATE;
    rs->round_number++;
}
