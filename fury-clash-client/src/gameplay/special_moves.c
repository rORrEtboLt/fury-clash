#include "special_moves.h"
#include "../input/input.h"
#include <string.h>

/* Convert raw input bitfield + facing to numpad direction (1-9) */
static uint8_t input_to_numpad(uint16_t input, int facing) {
    int up    = (input >> 0) & 1;
    int down  = (input >> 1) & 1;
    int back  = facing > 0 ? (input >> 2) & 1 : (input >> 3) & 1;
    int fwd   = facing > 0 ? (input >> 3) & 1 : (input >> 2) & 1;

    if (down && fwd)  return 3;
    if (down && back) return 1;
    if (up   && fwd)  return 9;
    if (up   && back) return 7;
    if (down)         return 2;
    if (up)           return 8;
    if (fwd)          return 6;
    if (back)         return 4;
    return 5;
}

void motion_detector_init(MotionDetector *md) {
    memset(md, 0, sizeof(*md));
    /* Fill history with neutral */
    for (int i = 0; i < 30; i++) md->dir_history[i] = 5;
}

void motion_detector_push(MotionDetector *md, uint16_t input, int facing) {
    md->dir_history[md->head] = input_to_numpad(input, facing);
    md->head = (md->head + 1) % 30;

    /* Track charge frames */
    uint8_t np = input_to_numpad(input, facing);
    md->charge_left_frames  = (np == 4 || np == 1 || np == 7) ? md->charge_left_frames  + 1 : 0;
    md->charge_down_frames  = (np == 2 || np == 1 || np == 3) ? md->charge_down_frames + 1 : 0;
}

/* Search for a sequence in the rolling history within the last `window` frames */
static int find_sequence(const MotionDetector *md, const uint8_t *seq, int seq_len, int window) {
    int matched = 0;
    for (int i = 0; i < window && matched < seq_len; i++) {
        int idx = ((md->head - 1 - i) + 30) % 30;
        if (md->dir_history[idx] == seq[seq_len - 1 - matched])
            matched++;
    }
    return matched == seq_len;
}

MotionCmd motion_detector_check(const MotionDetector *md) {
    static const uint8_t qcf[] = { 2, 3, 6 };
    static const uint8_t qcb[] = { 2, 1, 4 };
    static const uint8_t dp[]  = { 6, 2, 3 };
    static const uint8_t rdp[] = { 4, 2, 1 };

    if (find_sequence(md, qcf, 3, 12)) return MC_QCF;
    if (find_sequence(md, dp,  3, 15)) return MC_DP;
    if (find_sequence(md, qcb, 3, 12)) return MC_QCB;
    if (find_sequence(md, rdp, 3, 15)) return MC_RDP;

    /* Charge forward: held back 40+ frames, now forward */
    if (md->charge_left_frames == 0) {
        /* check if we were charging */
        int prev_idx = ((md->head - 2) + 30) % 30;
        if (md->dir_history[prev_idx] == 6) return MC_CHARGE_F;
    }

    return MC_NONE;
}
