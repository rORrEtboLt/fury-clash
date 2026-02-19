#pragma once
#include "fighter.h"

/* Move table entry — loaded from assets/fighters/<name>/data.json */
typedef struct MoveData {
    const char  *move_id;
    int          total_frames;
    int          startup;
    int          active;
    int          recovery;
    HitProperties hit;
    /* Hitboxes per active frame (simplified: single box per move) */
    Rect         hitbox;
} MoveData;

typedef struct FighterData {
    const char *name;
    int         walk_speed;
    int         back_walk_speed;
    float       jump_vy;
    float       jump_vx;
    MoveData   *moves;
    int         move_count;
} FighterData;

/* Returns fighter data for the given fighter_id, loaded from JSON at startup */
const FighterData *fighter_data_get(int fighter_id);
int  fighter_data_load_all(void);   /* call once at startup */
