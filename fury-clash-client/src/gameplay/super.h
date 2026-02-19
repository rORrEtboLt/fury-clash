#pragma once
#include "fighter.h"

#define FC_SUPER_COST 1.0f  /* full meter */

void super_build_meter(Fighter *f, int damage_dealt, int damage_received);
int  super_can_activate(const Fighter *f);
void super_activate(Fighter *f);
