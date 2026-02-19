#include "fighter.h"
#include "physics.h"
#include "fighter_data.h"
#include "../core/config.h"
#include <string.h>
#include <math.h>

void fighter_init(Fighter *f, int fighter_id, int player_slot) {
    memset(f, 0, sizeof(*f));
    f->fighter_id  = fighter_id;
    f->player_slot = player_slot;
    f->facing      = (player_slot == 0) ? 1 : -1;
    f->x           = (player_slot == 0) ? 300.0f : 980.0f;
    f->y           = FC_GROUND_Y;
    f->health      = FC_MAX_HEALTH;
    f->max_health  = FC_MAX_HEALTH;
    f->super_meter = 0.0f;
    f->state       = FS_IDLE;

    /* Hurtbox — full standing body */
    f->hurtbox = (Rect){ -30, -160, 60, 160 };
}

static int is_airborne(const Fighter *f) {
    return f->y < FC_GROUND_Y - 1.0f;
}

void fighter_tick(Fighter *f, uint16_t input, const Fighter *opponent) {
    /* Decrement stun timers */
    if (f->hitstun  > 0) { f->hitstun--;  return; }
    if (f->blockstun > 0) { f->blockstun--; return; }

    /* Update facing toward opponent */
    float dx = opponent->x - f->x;
    if (f->state == FS_IDLE || f->state == FS_WALK_FWD || f->state == FS_WALK_BACK)
        f->facing = (dx >= 0) ? 1 : -1;

    /* Physics */
    physics_apply_gravity(f);
    physics_move(f);
    physics_clamp_to_stage(f);

    /* Basic input-to-state mapping (expanded by special_moves.c) */
    int press_fwd  = (input >> 3) & 1;   /* right */
    int press_back = (input >> 2) & 1;   /* left  */
    /* Direction relative to facing */
    int move_fwd   = f->facing > 0 ? press_fwd : press_back;

    if (f->state == FS_IDLE || f->state == FS_WALK_FWD || f->state == FS_WALK_BACK) {
        if (move_fwd)       { f->state = FS_WALK_FWD; f->vx =  200.0f * f->facing; }
        else if (!move_fwd && (press_fwd || press_back))
                            { f->state = FS_WALK_BACK; f->vx = -150.0f * f->facing; }
        else                { f->state = FS_IDLE; f->vx = 0; }
    }

    f->state_frame++;
}

void fighter_apply_hit(Fighter *f, const HitProperties *hp, int blocked) {
    if (blocked) {
        f->health     -= hp->chip_damage;
        f->blockstun   = hp->blockstun;
        f->vx          = -hp->knockback_x * 0.3f;
    } else {
        f->health     -= hp->damage;
        f->hitstun     = hp->hitstun;
        f->vx          = -hp->knockback_x;
        f->vy          = hp->knockback_y;
        f->combo_counter++;
        f->state       = FS_HIT;
        f->state_frame = 0;
    }
    if (f->health < 0) f->health = 0;
}
