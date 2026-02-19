#include "camera.h"
#include "../core/config.h"
#include <math.h>
#include <stdlib.h>

void camera_init(Camera *cam) {
    cam->x            = FC_GAME_W / 2.0f;
    cam->y            = FC_GAME_H / 2.0f;
    cam->zoom         = 1.0f;
    cam->shake_x      = 0;
    cam->shake_y      = 0;
    cam->freeze_frames = 0;
}

void camera_update(Camera *cam, const Fighter *p1, const Fighter *p2) {
    if (cam->freeze_frames > 0) { cam->freeze_frames--; return; }

    float cx = (p1->x + p2->x) / 2.0f;
    float cy = (p1->y + p2->y) / 2.0f - 30.0f;

    float dist = fabsf(p1->x - p2->x);
    float target_zoom = fmaxf(FC_CAM_ZOOM_MIN, fminf(FC_CAM_ZOOM_MAX, 500.0f / dist));

    cam->x    += (cx - cam->x)              * FC_CAM_LERP;
    cam->y    += (cy - cam->y)              * FC_CAM_LERP;
    cam->zoom += (target_zoom - cam->zoom)  * FC_CAM_ZOOM_LERP;

    cam->shake_x *= 0.85f;
    cam->shake_y *= 0.85f;
}

void camera_add_shake(Camera *cam, float intensity) {
    cam->shake_x += ((float)(rand() % 200 - 100) / 100.0f) * intensity;
    cam->shake_y += ((float)(rand() % 200 - 100) / 100.0f) * intensity;
}

void camera_trigger_freeze(Camera *cam, int frames) {
    cam->freeze_frames = frames;
}
