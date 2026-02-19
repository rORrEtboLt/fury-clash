#include "gameloop.h"
#include "config.h"
#include <SDL3/SDL.h>

void gameloop_init(GameLoop *gl) {
    gl->accumulator   = 0.0;
    gl->tick_dt       = FC_TICK_DT;
    gl->last_counter  = SDL_GetPerformanceCounter();
    gl->perf_freq     = SDL_GetPerformanceFrequency();
    gl->current_frame = 0;
    gl->running       = 1;
}

void gameloop_begin_frame(GameLoop *gl, double *out_dt) {
    Uint64 now = SDL_GetPerformanceCounter();
    double dt  = (double)(now - gl->last_counter) / (double)gl->perf_freq;
    gl->last_counter = now;

    /* Clamp large spikes (e.g. debugger break, OS scheduler jitter) */
    if (dt > 0.05) dt = 0.05;

    gl->accumulator += dt;
    if (out_dt) *out_dt = dt;
}

int gameloop_should_tick(GameLoop *gl) {
    if (gl->accumulator >= gl->tick_dt) {
        gl->accumulator -= gl->tick_dt;
        gl->current_frame++;
        return 1;
    }
    return 0;
}

float gameloop_render_alpha(const GameLoop *gl) {
    return (float)(gl->accumulator / gl->tick_dt);
}
