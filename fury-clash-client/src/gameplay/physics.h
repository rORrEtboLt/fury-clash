#pragma once
#include "fighter.h"

void physics_apply_gravity(Fighter *f);
void physics_move(Fighter *f);
void physics_clamp_to_stage(Fighter *f);
void physics_push_apart(Fighter *p1, Fighter *p2);  /* prevent overlap */
