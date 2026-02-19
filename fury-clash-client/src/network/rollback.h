#pragma once
#include <stdint.h>
#include "../gameplay/simulation.h"

/*
 * Rollback engine.
 *
 * Each frame:
 *   1. Predict remote input (repeat last known).
 *   2. Simulate + save snapshot.
 *   3. Send local input to server.
 *
 * When confirmed remote input arrives and differs from prediction:
 *   1. Rewind to the divergence frame.
 *   2. Re-simulate forward to current frame.
 *   3. Carry on.
 */

void     rollback_init(void);
void     rollback_shutdown(void);

/* Called each sim tick: save current state at frame */
void     rollback_save(int frame);

/* Returns the input to use for the remote player at `frame` (predict if unknown) */
uint16_t rollback_get_remote(int frame);

/* Called when a confirmed remote input arrives */
void     rollback_confirm_remote(int frame, uint16_t input);

/* Returns 1 if a re-simulation is needed */
int      rollback_needs_resim(void);

/* Rewind to divergence frame and re-simulate forward to current_frame */
void     rollback_rewind_and_resim(int current_frame);
