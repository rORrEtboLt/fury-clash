#include "rollback.h"
#include "input_buffer.h"
#include "../gameplay/simulation.h"
#include "../core/config.h"
#include <string.h>
#include <SDL3/SDL.h>

typedef struct FrameEntry {
    SimState   snapshot;
    uint16_t   local_input;
    uint16_t   remote_input;
    int        remote_confirmed;
} FrameEntry;

static FrameEntry g_buf[FC_ROLLBACK_FRAMES];
static int        g_diverge_frame = -1;

void rollback_init(void) {
    memset(g_buf, 0, sizeof(g_buf));
    g_diverge_frame = -1;
}

void rollback_shutdown(void) { /* no-op */ }

void rollback_save(int frame) {
    FrameEntry *e = &g_buf[frame % FC_ROLLBACK_FRAMES];
    simulation_save(&e->snapshot);
    e->local_input       = 0;
    e->remote_confirmed  = 0;
}

uint16_t rollback_get_remote(int frame) {
    FrameEntry *e = &g_buf[frame % FC_ROLLBACK_FRAMES];
    if (e->remote_confirmed) return e->remote_input;

    /* Predict: repeat last confirmed remote input */
    int prev = frame - 1;
    if (prev >= 0) {
        FrameEntry *pe = &g_buf[prev % FC_ROLLBACK_FRAMES];
        return pe->remote_input; /* last known, whether confirmed or predicted */
    }
    return 0; /* neutral */
}

void rollback_confirm_remote(int frame, uint16_t input) {
    FrameEntry *e = &g_buf[frame % FC_ROLLBACK_FRAMES];
    if (!e->remote_confirmed && e->remote_input != input) {
        /* Mismatch — mark divergence */
        if (g_diverge_frame < 0 || frame < g_diverge_frame)
            g_diverge_frame = frame;
    }
    e->remote_input     = input;
    e->remote_confirmed = 1;
}

int rollback_needs_resim(void) {
    return g_diverge_frame >= 0;
}

void rollback_rewind_and_resim(int current_frame) {
    if (g_diverge_frame < 0) return;

    SDL_Log("[rollback] rewind from %d to %d", current_frame, g_diverge_frame);

    /* Restore state at divergence point */
    FrameEntry *base = &g_buf[g_diverge_frame % FC_ROLLBACK_FRAMES];
    simulation_restore(&base->snapshot);

    /* Re-simulate forward */
    for (int f = g_diverge_frame; f < current_frame; f++) {
        FrameEntry *e = &g_buf[f % FC_ROLLBACK_FRAMES];
        simulation_tick(e->local_input, e->remote_input);
        simulation_save(&e->snapshot);
    }

    g_diverge_frame = -1;
}
