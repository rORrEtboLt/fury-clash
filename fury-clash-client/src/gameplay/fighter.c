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

/* First move index that is an air attack — must match fighter_data.c layout. */
#define JUMP_MOVE_FIRST 3

void fighter_tick(Fighter *f, uint16_t input, const Fighter *opponent) {
    /* Decrement stun timers */
    if (f->hitstun  > 0) { f->hitstun--;  return; }
    if (f->blockstun > 0) { f->blockstun--; return; }

    /* Update facing toward opponent (ground states only; can't turn mid-air) */
    float dx = opponent->x - f->x;
    if (f->state == FS_IDLE || f->state == FS_WALK_FWD || f->state == FS_WALK_BACK)
        f->facing = (dx >= 0) ? 1 : -1;

    /* Physics */
    physics_apply_gravity(f);
    physics_move(f);
    physics_clamp_to_stage(f);

    int airborne = (f->y < FC_GROUND_Y - 1.0f);

    /* Landing while performing an air attack: cancel to idle immediately */
    if (!airborne && f->state == FS_ATTACK && f->move_id >= JUMP_MOVE_FIRST) {
        f->state        = FS_IDLE;
        f->state_frame  = 0;
        f->hitbox_count = 0;
        f->vx           = 0;
    }

    /* Edge-detection: bits set only on the frame the button was first pressed */
    uint16_t just_pressed = input & ~f->prev_input;

    int press_up   = (input       >> 0) & 1;
    int press_fwd  = (input       >> 3) & 1;
    int press_back = (input       >> 2) & 1;
    int move_fwd   = f->facing > 0 ? press_fwd : press_back;

    /* ── Attack state: advance hitbox window and check for move end ── */
    if (f->state == FS_ATTACK) {
        const FighterData *fd = fighter_data_get(f->fighter_id);
        if (fd && f->move_id < fd->move_count) {
            const MoveData *mv = &fd->moves[f->move_id];
            /* Activate hitbox during the active window */
            if (f->state_frame >= mv->startup &&
                f->state_frame <  mv->startup + mv->active) {
                f->hitboxes[0]  = mv->hitbox;
                f->hitbox_count = 1;
                f->hit_props    = mv->hit;
            } else {
                f->hitbox_count = 0;
            }
            /* End of move: return to appropriate state */
            if (f->state_frame >= mv->total_frames) {
                f->state        = airborne ? FS_JUMP_NEUTRAL : FS_IDLE;
                f->state_frame  = 0;
                f->hitbox_count = 0;
            }
        }
        f->state_frame++;
        f->prev_input = input;
        return;
    }

    /* ── Ground movement + jump launch ── */
    if (f->state == FS_IDLE || f->state == FS_WALK_FWD || f->state == FS_WALK_BACK) {
        if (press_up) {
            const FighterData *fd = fighter_data_get(f->fighter_id);
            float jvy = fd ? fd->jump_vy : -900.0f;
            float jvx = fd ? fd->jump_vx : 180.0f;
            f->vy = jvy;
            if (move_fwd) {
                f->state = FS_JUMP_FWD;
                f->vx    =  jvx * (float)f->facing;
            } else if (press_fwd || press_back) {
                f->state = FS_JUMP_BACK;
                f->vx    = -jvx * (float)f->facing;
            } else {
                f->state = FS_JUMP_NEUTRAL;
                f->vx    = 0;
            }
            f->state_frame = 0;
        } else {
            if (move_fwd)                              { f->state = FS_WALK_FWD;  f->vx =  200.0f * (float)f->facing; }
            else if (press_fwd || press_back)          { f->state = FS_WALK_BACK; f->vx = -150.0f * (float)f->facing; }
            else                                       { f->state = FS_IDLE;      f->vx = 0; }
        }
    }

    /* ── Jump punch: LP or HP on first pressed frame while airborne ── */
    if (airborne &&
        (f->state == FS_JUMP_NEUTRAL ||
         f->state == FS_JUMP_FWD     ||
         f->state == FS_JUMP_BACK)) {
        int lp = (just_pressed >> 4) & 1;   /* INPUT_LIGHT_PUNCH */
        int hp = (just_pressed >> 5) & 1;   /* INPUT_HEAVY_PUNCH */
        if (lp || hp) {
            const FighterData *fd = fighter_data_get(f->fighter_id);
            if (fd) {
                int mid = hp ? 4 : 3;       /* jump_HP = 4, jump_LP = 3 */
                if (mid < fd->move_count) {
                    f->state        = FS_ATTACK;
                    f->move_id      = mid;
                    f->state_frame  = 0;
                    f->hitbox_count = 0;
                }
            }
        }
    }

    f->state_frame++;
    f->prev_input = input;
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
