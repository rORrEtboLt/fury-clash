#include "hitbox.h"
#include <string.h>

static int rects_overlap(float ax, float ay, float aw, float ah,
                          float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

int hitbox_check(const Fighter *attacker, const Fighter *defender) {
    if (attacker->hitbox_count == 0) return 0;

    /* World-space hurtbox of defender */
    float hx = defender->x + defender->hurtbox.x;
    float hy = defender->y + defender->hurtbox.y;
    float hw = defender->hurtbox.w;
    float hh = defender->hurtbox.h;

    for (int i = 0; i < attacker->hitbox_count; i++) {
        const Rect *hb = &attacker->hitboxes[i];
        /* World-space hitbox (flip x if facing left) */
        float wx = attacker->x + hb->x * attacker->facing;
        float wy = attacker->y + hb->y;
        if (rects_overlap(wx, wy, hb->w, hb->h, hx, hy, hw, hh))
            return 1;
    }
    return 0;
}

void hitbox_resolve(Fighter *attacker, Fighter *defender) {
    int blocked = (defender->state == FS_BLOCK || defender->state == FS_CROUCH);
    fighter_apply_hit(defender, &attacker->hit_props, blocked);
    /* Clear hitboxes so the hit only fires once per active window */
    attacker->hitbox_count = 0;
}
