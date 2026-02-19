#pragma once
#include <SDL3/SDL.h>
#include <stdint.h>

typedef struct TouchLayout {
    float screen_w, screen_h;
    float safe_left, safe_right, safe_top, safe_bottom;
    float dpi_scale;

    /* Computed regions */
    SDL_FRect viewport;
    SDL_FRect joystick_zone;
    SDL_FRect button_zone;
    float     button_radius;
    float     joystick_radius;
    int       is_tablet;
} TouchLayout;

void     input_touch_init(void);
void     input_touch_shutdown(void);
void     input_touch_layout(float screen_w, float screen_h,
                             float dpi_scale, int is_tablet);
void     input_touch_event(const SDL_Event *ev);
uint16_t input_touch_read(void);

/* Called by renderer to draw the virtual controller overlay */
const TouchLayout *input_touch_get_layout(void);
