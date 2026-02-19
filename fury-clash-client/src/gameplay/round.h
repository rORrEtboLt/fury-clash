#pragma once

typedef struct RoundState {
    int  p1_rounds_won;
    int  p2_rounds_won;
    int  frame_timer;        /* counts down from FC_ROUND_TIME_SEC * 60 */
    int  round_number;
    int  round_over;
    int  match_over;
    int  winner_slot;        /* -1 = draw, 0 = P1, 1 = P2 */
} RoundState;

void round_init(RoundState *rs);
void round_tick(RoundState *rs, int p1_hp, int p2_hp);  /* call each sim tick */
void round_start_next(RoundState *rs);
