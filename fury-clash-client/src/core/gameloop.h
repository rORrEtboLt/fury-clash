#pragma once
#include <SDL3/SDL.h>

typedef struct GameLoop {
    double   accumulator;
    double   tick_dt;       /* 1.0 / tick_rate */
    Uint64   last_counter;
    Uint64   perf_freq;
    int      current_frame;
    int      running;
} GameLoop;

void gameloop_init(GameLoop *gl);
void gameloop_begin_frame(GameLoop *gl, double *out_dt);

/* Returns 1 each time a simulation tick should fire */
int  gameloop_should_tick(GameLoop *gl);

/* Call after all ticks; returns blend alpha for interpolated render */
float gameloop_render_alpha(const GameLoop *gl);
