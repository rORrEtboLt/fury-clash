#pragma once
#include <SDL3/SDL.h>

typedef struct Timer {
    Uint64 start;
    Uint64 freq;
} Timer;

void   timer_init(Timer *t);
double timer_elapsed_ms(const Timer *t);
void   timer_reset(Timer *t);
