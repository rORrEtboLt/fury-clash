#pragma once
#include <stdint.h>

/*
 * Motion command detector.
 * Maintains a rolling 30-frame directional history and detects
 * QCF (236), DP (623), QCB (214), charge moves, etc.
 */

typedef enum MotionCmd {
    MC_NONE = 0,
    MC_QCF,       /* ↓↘→ */
    MC_QCB,       /* ↓↙← */
    MC_DP,        /* →↓↘ */
    MC_RDP,       /* ←↓↙ */
    MC_CHARGE_F,  /* hold ← then → */
    MC_CHARGE_U,  /* hold ↓ then ↑ */
    MC_360,       /* full rotation */
} MotionCmd;

typedef struct MotionDetector {
    uint8_t dir_history[30]; /* numpad notation: 5=neutral, 2=down, 6=right… */
    int     head;
    int     charge_left_frames;
    int     charge_down_frames;
} MotionDetector;

void      motion_detector_init(MotionDetector *md);
void      motion_detector_push(MotionDetector *md, uint16_t input, int facing);
MotionCmd motion_detector_check(const MotionDetector *md);
