#include "state_machine.h"
#include "config.h"
#include <SDL3/SDL.h>

void statemachine_init(StateMachine *sm) {
    sm->current          = GS_TITLE;
    sm->pending          = GS_COUNT;
    sm->transition_frame = 0;
}

void statemachine_request(StateMachine *sm, GameState next) {
    if (next < GS_COUNT)
        sm->pending = next;
}

void statemachine_tick(StateMachine *sm) {
    if (sm->pending != GS_COUNT) {
        SDL_Log("[state] %d -> %d", sm->current, sm->pending);
        sm->current          = sm->pending;
        sm->pending          = GS_COUNT;
        sm->transition_frame = 0;
    }
    sm->transition_frame++;
}
