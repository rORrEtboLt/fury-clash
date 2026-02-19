#include "combo.h"
#include <string.h>

void combo_reset(ComboTracker *ct) {
    memset(ct, 0, sizeof(*ct));
    ct->damage_scaling = 100;
}

void combo_on_hit(ComboTracker *ct, int is_launch) {
    ct->active = 1;
    ct->hit_count++;
    if (is_launch) ct->juggle_count++;

    /* Standard damage scaling curve */
    if      (ct->hit_count <= 2)  ct->damage_scaling = 100;
    else if (ct->hit_count <= 4)  ct->damage_scaling = 90;
    else if (ct->hit_count <= 6)  ct->damage_scaling = 75;
    else if (ct->hit_count <= 9)  ct->damage_scaling = 60;
    else                          ct->damage_scaling = 50;
}

int combo_damage_scaled(const ComboTracker *ct, int base_damage) {
    return base_damage * ct->damage_scaling / 100;
}

int combo_can_juggle(const ComboTracker *ct, int move_juggle_value) {
    return ct->juggle_count < move_juggle_value;
}
