#pragma once

typedef enum GameState {
    GS_TITLE = 0,
    GS_MAIN_MENU,
    GS_CHAR_SELECT,
    GS_MATCH_SEARCH,
    GS_LOADING,
    GS_FIGHT,
    GS_RESULTS,
    GS_TRAINING,
    GS_SETTINGS,
    GS_COUNT
} GameState;

typedef struct StateMachine {
    GameState current;
    GameState pending;      /* set to GS_COUNT if no transition */
    int       transition_frame;
} StateMachine;

void statemachine_init(StateMachine *sm);
void statemachine_request(StateMachine *sm, GameState next);
void statemachine_tick(StateMachine *sm);  /* applies pending transition */
