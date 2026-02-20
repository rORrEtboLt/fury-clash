#include "platform.h"
#include <SDL3/SDL.h>
#include <string.h>

void platform_init(PlatformInfo *info) {
    memset(info, 0, sizeof(*info));

    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (mode) {
        info->screen_w = (float)mode->w;
        info->screen_h = (float)mode->h;
    }

    /* DPI */
    SDL_GetDisplayContentScale(display); /* SDL3: content scale instead */
    info->screen_dpi = 96.0f; /* default desktop DPI */

    /* Platform tag */
#if defined(__APPLE__)
    info->platform = "ios";
#elif defined(__ANDROID__)
    info->platform = "android";
#else
    info->platform = "desktop";
#endif

    /* Safe area — desktop has no notch */
    info->safe_area_top    = 0;
    info->safe_area_bottom = 0;
    info->safe_area_left   = 0;
    info->safe_area_right  = 0;
    info->is_tablet        = 0;
    info->has_notch        = 0;
}

void platform_open_url(const char *url) {
    SDL_OpenURL(url);
}

void platform_haptic_light(void)  { /* no-op on desktop */ }
void platform_haptic_heavy(void)  { /* no-op on desktop */ }
