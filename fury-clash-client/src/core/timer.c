#include "timer.h"

void timer_init(Timer *t) {
    t->freq  = SDL_GetPerformanceFrequency();
    t->start = SDL_GetPerformanceCounter();
}

double timer_elapsed_ms(const Timer *t) {
    Uint64 now = SDL_GetPerformanceCounter();
    return (double)(now - t->start) * 1000.0 / (double)t->freq;
}

void timer_reset(Timer *t) {
    t->start = SDL_GetPerformanceCounter();
}
