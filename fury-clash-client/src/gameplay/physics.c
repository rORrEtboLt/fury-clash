#include "physics.h"
#include "../core/config.h"
#include <math.h>

void physics_apply_gravity(Fighter *f) {
    if (f->y < FC_GROUND_Y) {
        f->vy += FC_GRAVITY * FC_TICK_DT;
    }
}

void physics_move(Fighter *f) {
    f->x += f->vx * (float)FC_TICK_DT;
    f->y += f->vy * (float)FC_TICK_DT;

    /* Ground check */
    if (f->y >= FC_GROUND_Y) {
        f->y   = FC_GROUND_Y;
        f->vy  = 0;
        if (f->state == FS_JUMP_NEUTRAL || f->state == FS_JUMP_FWD || f->state == FS_JUMP_BACK) {
            f->state       = FS_IDLE;
            f->state_frame = 0;
            f->vx          = 0;
        }
    }
}

void physics_clamp_to_stage(Fighter *f) {
    if (f->x < FC_STAGE_LEFT)  f->x = FC_STAGE_LEFT;
    if (f->x > FC_STAGE_RIGHT) f->x = FC_STAGE_RIGHT;
}

void physics_push_apart(Fighter *p1, Fighter *p2) {
    float overlap_threshold = 60.0f;
    float dx = p2->x - p1->x;
    float dist = fabsf(dx);
    if (dist < overlap_threshold && dist > 0.1f) {
        float push = (overlap_threshold - dist) * 0.5f;
        float dir  = (dx > 0) ? 1.0f : -1.0f;
        p1->x -= push * dir;
        p2->x += push * dir;
        physics_clamp_to_stage(p1);
        physics_clamp_to_stage(p2);
    }
}
