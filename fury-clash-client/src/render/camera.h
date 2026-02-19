#pragma once
#include "../gameplay/fighter.h"

typedef struct Camera {
    float x, y;
    float zoom;
    float shake_x, shake_y;
    int   freeze_frames;
} Camera;

void camera_init(Camera *cam);
void camera_update(Camera *cam, const Fighter *p1, const Fighter *p2);
void camera_add_shake(Camera *cam, float intensity);
void camera_trigger_freeze(Camera *cam, int frames);
