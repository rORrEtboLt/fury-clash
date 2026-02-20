#pragma once
#include <stdint.h>

typedef struct { float x, y, w, h; } Rect;

typedef struct {
    int   damage;
    int   chip_damage;
    int   hitstun;
    int   blockstun;
    float knockback_x, knockback_y;
    int   launches;     /* air juggle */
} HitProperties;

typedef enum FighterState {
    FS_IDLE = 0,
    FS_WALK_FWD,
    FS_WALK_BACK,
    FS_CROUCH,
    FS_JUMP_NEUTRAL,
    FS_JUMP_FWD,
    FS_JUMP_BACK,
    FS_ATTACK,          /* generic; state_frame maps to move */
    FS_HIT,
    FS_BLOCK,
    FS_KNOCKDOWN,
    FS_WAKEUP,
    FS_COUNT
} FighterState;

typedef struct Fighter {
    /* Identity */
    int         fighter_id;     /* character index (0 = ryu, 1 = ken…) */
    int         player_slot;    /* 0 = P1, 1 = P2 */

    /* Transform */
    float       x, y;
    float       vx, vy;
    int         facing;         /* +1 right, -1 left */

    /* Combat state machine */
    FighterState state;
    int          state_frame;
    int          hitstun;
    int          blockstun;
    int          combo_counter;
    int          move_id;       /* current attack move index */

    /* Stats */
    int          health;
    int          max_health;
    float        super_meter;   /* 0.0 → 1.0 */

    /* Animation */
    int          anim_id;
    int          anim_frame;

    /* Hitboxes (active during attacks) */
    Rect         hitboxes[4];
    int          hitbox_count;
    Rect         hurtbox;
    HitProperties hit_props;

    /* Input history — part of rollback state for deterministic edge detection */
    uint16_t     prev_input;
} Fighter;

void fighter_init(Fighter *f, int fighter_id, int player_slot);
void fighter_tick(Fighter *f, uint16_t input, const Fighter *opponent);
void fighter_apply_hit(Fighter *f, const HitProperties *hp, int blocked);
