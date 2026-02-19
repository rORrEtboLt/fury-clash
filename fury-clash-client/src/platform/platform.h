#pragma once

/* Cross-platform info populated at startup */
typedef struct PlatformInfo {
    float safe_area_top, safe_area_bottom;
    float safe_area_left, safe_area_right;
    float screen_dpi;
    float screen_w, screen_h;
    int   is_tablet;       /* iPad or Android tablet */
    int   has_notch;
    const char *platform;  /* "ios", "android", "desktop" */
} PlatformInfo;

void platform_init(PlatformInfo *info);
void platform_open_url(const char *url);
void platform_haptic_light(void);
void platform_haptic_heavy(void);
