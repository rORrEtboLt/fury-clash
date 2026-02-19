#include "super.h"

void super_build_meter(Fighter *f, int damage_dealt, int damage_received) {
    /* Gain meter on both dealing and receiving damage */
    f->super_meter += (float)damage_dealt    * 0.0005f;
    f->super_meter += (float)damage_received * 0.001f;
    if (f->super_meter > 1.0f) f->super_meter = 1.0f;
}

int super_can_activate(const Fighter *f) {
    return f->super_meter >= FC_SUPER_COST;
}

void super_activate(Fighter *f) {
    f->super_meter -= FC_SUPER_COST;
    if (f->super_meter < 0.0f) f->super_meter = 0.0f;
    /* Transition to super attack state is handled by fighter state machine */
}
