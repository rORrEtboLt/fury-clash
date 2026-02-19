#pragma once
#include "fighter.h"

/* Returns 1 if hitbox of attacker overlaps hurtbox of defender */
int  hitbox_check(const Fighter *attacker, const Fighter *defender);

/* Resolve the hit: apply damage, stun, knockback */
void hitbox_resolve(Fighter *attacker, Fighter *defender);
