#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>

/*
 * Unified input interface.
 * Sources: touch, gamepad, keyboard (all map to same 16-bit bitfield).
 */

void     input_init(void);
void     input_shutdown(void);
void     input_handle_event(const SDL_Event *ev);
void     input_tick(void);           /* call once per sim tick */

uint16_t input_get_local(void);      /* current frame bitfield for local player */

/* Touch-specific layout update (called on window resize) */
void     input_touch_layout_update(float screen_w, float screen_h,
                                    float dpi_scale, int is_tablet);
