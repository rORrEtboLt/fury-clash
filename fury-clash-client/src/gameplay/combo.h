#pragma once
#include "fighter.h"

/* Combo tracking — tracks whether a cancel is allowed and juggle limit */
typedef struct ComboTracker {
    int active;          /* 1 if combo is ongoing */
    int hit_count;
    int juggle_count;    /* times target has been launched */
    int damage_scaling;  /* % of base damage applied (100 = full, decays) */
} ComboTracker;

void combo_reset(ComboTracker *ct);
void combo_on_hit(ComboTracker *ct, int is_launch);
int  combo_damage_scaled(const ComboTracker *ct, int base_damage);
int  combo_can_juggle(const ComboTracker *ct, int move_juggle_value);
