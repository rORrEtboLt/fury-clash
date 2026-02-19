/*
 * SDL3 Neon Dodge - A simple arcade game prototype
 * Build: see build.sh or Makefile
 *
 * Controls:
 *   Arrow keys / WASD  - Move the ship
 *   Space               - Restart after game over
 *   Escape / Q          - Quit
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */
#define SCREEN_W    800
#define SCREEN_H    600
#define MAX_STARS   120
#define MAX_ROCKS    24
#define MAX_PARTICLES 200
#define PLAYER_SIZE  16
#define FPS          60

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */
typedef struct { float x, y, speed; Uint8 brightness; } Star;
typedef struct { float x, y, w, h, speed, angle, rot_speed; int active; } Rock;
typedef struct { float x, y, vx, vy, life; Uint8 r, g, b; int active; } Particle;

typedef struct {
    /* player */
    float px, py;
    float pvx, pvy;
    int alive;

    /* world */
    Star   stars[MAX_STARS];
    Rock   rocks[MAX_ROCKS];
    Particle particles[MAX_PARTICLES];

    float  difficulty;      /* scales over time */
    float  score;
    float  high_score;
    float  spawn_timer;

    /* input */
    int key_up, key_down, key_left, key_right;
} Game;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static float randf(float lo, float hi) {
    return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

static void spawn_particle(Game *g, float x, float y, Uint8 r, Uint8 gr, Uint8 b) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) {
            p->active = 1;
            p->x = x; p->y = y;
            p->vx = randf(-120, 120);
            p->vy = randf(-120, 120);
            p->life = randf(0.3f, 0.9f);
            p->r = r; p->g = gr; p->b = b;
            return;
        }
    }
}

static void spawn_explosion(Game *g, float x, float y) {
    for (int i = 0; i < 40; i++)
        spawn_particle(g, x, y,
            (Uint8)(200 + rand()%56),
            (Uint8)(80 + rand()%120),
            (Uint8)(20 + rand()%60));
}

static void spawn_trail(Game *g, float x, float y) {
    spawn_particle(g, x, y, 60, 180, 255);
}

static void init_stars(Game *g) {
    for (int i = 0; i < MAX_STARS; i++) {
        g->stars[i].x = randf(0, SCREEN_W);
        g->stars[i].y = randf(0, SCREEN_H);
        g->stars[i].speed = randf(20, 100);
        g->stars[i].brightness = (Uint8)(100 + rand() % 156);
    }
}

static void spawn_rock(Game *g) {
    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock *r = &g->rocks[i];
        if (!r->active) {
            r->active = 1;
            r->w = randf(20, 50);
            r->h = randf(20, 50);
            /* come from top or sides */
            int side = rand() % 3;
            if (side == 0) { r->x = randf(-40, SCREEN_W); r->y = -r->h; }
            else if (side == 1) { r->x = -r->w; r->y = randf(-40, SCREEN_H * 0.5f); }
            else { r->x = SCREEN_W; r->y = randf(-40, SCREEN_H * 0.5f); }
            r->speed = randf(80, 180) * g->difficulty;
            r->angle = randf(0, 360);
            r->rot_speed = randf(-180, 180);
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Game reset                                                          */
/* ------------------------------------------------------------------ */
static void game_reset(Game *g) {
    g->px = SCREEN_W / 2.0f;
    g->py = SCREEN_H * 0.75f;
    g->pvx = g->pvy = 0;
    g->alive = 1;
    g->difficulty = 1.0f;
    g->score = 0;
    g->spawn_timer = 0;
    memset(g->rocks, 0, sizeof(g->rocks));
    memset(g->particles, 0, sizeof(g->particles));
    g->key_up = g->key_down = g->key_left = g->key_right = 0;
    init_stars(g);
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */
static void game_update(Game *g, float dt) {
    /* stars scroll */
    for (int i = 0; i < MAX_STARS; i++) {
        g->stars[i].y += g->stars[i].speed * dt;
        if (g->stars[i].y > SCREEN_H) {
            g->stars[i].y = 0;
            g->stars[i].x = randf(0, SCREEN_W);
        }
    }

    /* particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) continue;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->life -= dt;
        if (p->life <= 0) p->active = 0;
    }

    if (!g->alive) return;

    /* player movement */
    float accel = 800.0f, friction = 5.0f, max_speed = 320.0f;
    float ax = 0, ay = 0;
    if (g->key_left)  ax -= accel;
    if (g->key_right) ax += accel;
    if (g->key_up)    ay -= accel;
    if (g->key_down)  ay += accel;
    g->pvx += ax * dt;  g->pvy += ay * dt;
    g->pvx -= g->pvx * friction * dt;
    g->pvy -= g->pvy * friction * dt;

    /* clamp speed */
    float spd = sqrtf(g->pvx * g->pvx + g->pvy * g->pvy);
    if (spd > max_speed) { g->pvx *= max_speed / spd; g->pvy *= max_speed / spd; }

    g->px += g->pvx * dt;
    g->py += g->pvy * dt;

    /* clamp position */
    if (g->px < PLAYER_SIZE) g->px = PLAYER_SIZE;
    if (g->px > SCREEN_W - PLAYER_SIZE) g->px = SCREEN_W - PLAYER_SIZE;
    if (g->py < PLAYER_SIZE) g->py = PLAYER_SIZE;
    if (g->py > SCREEN_H - PLAYER_SIZE) g->py = SCREEN_H - PLAYER_SIZE;

    /* engine trail */
    if (rand() % 3 == 0) spawn_trail(g, g->px + randf(-4, 4), g->py + PLAYER_SIZE);

    /* difficulty ramp */
    g->difficulty += 0.04f * dt;
    g->score += 100.0f * dt * g->difficulty;

    /* spawn rocks */
    g->spawn_timer -= dt;
    if (g->spawn_timer <= 0) {
        spawn_rock(g);
        g->spawn_timer = randf(0.3f, 1.2f) / g->difficulty;
    }

    /* update rocks */
    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock *r = &g->rocks[i];
        if (!r->active) continue;
        /* move toward bottom-ish */
        float target_y = SCREEN_H + 80;
        float dx = 0, dy = target_y - r->y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0) { dx /= len; dy /= len; }
        r->x += dx * r->speed * dt;
        r->y += dy * r->speed * dt;
        r->angle += r->rot_speed * dt;

        /* off screen? */
        if (r->y > SCREEN_H + 80) { r->active = 0; continue; }

        /* collision with player (simple AABB) */
        float half_w = r->w * 0.45f, half_h = r->h * 0.45f;
        float rx1 = r->x - half_w, ry1 = r->y - half_h;
        float rx2 = r->x + half_w, ry2 = r->y + half_h;
        float px1 = g->px - PLAYER_SIZE * 0.7f, py1 = g->py - PLAYER_SIZE * 0.7f;
        float px2 = g->px + PLAYER_SIZE * 0.7f, py2 = g->py + PLAYER_SIZE * 0.7f;

        if (px1 < rx2 && px2 > rx1 && py1 < ry2 && py2 > ry1) {
            g->alive = 0;
            spawn_explosion(g, g->px, g->py);
            if (g->score > g->high_score) g->high_score = g->score;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Draw helpers (SDL3 renderer)                                        */
/* ------------------------------------------------------------------ */

/* Filled rotated rectangle (approximated with 4 triangles from center) */
static void draw_rotated_rect(SDL_Renderer *ren, float cx, float cy,
                              float w, float h, float angle_deg,
                              Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    float rad = angle_deg * (float)M_PI / 180.0f;
    float cosA = cosf(rad), sinA = sinf(rad);
    float hw = w / 2, hh = h / 2;

    /* four corners */
    float corners[4][2] = {
        { -hw, -hh }, { hw, -hh }, { hw, hh }, { -hw, hh }
    };
    SDL_FPoint pts[4];
    for (int i = 0; i < 4; i++) {
        pts[i].x = cx + corners[i][0] * cosA - corners[i][1] * sinA;
        pts[i].y = cy + corners[i][0] * sinA + corners[i][1] * cosA;
    }

    /* draw as two triangles */
    SDL_Vertex verts[6];
    int idx[] = {0,1,2, 0,2,3};
    for (int i = 0; i < 6; i++) {
        verts[i].position.x = pts[idx[i]].x;
        verts[i].position.y = pts[idx[i]].y;
        verts[i].color.r = r / 255.0f;
        verts[i].color.g = g / 255.0f;
        verts[i].color.b = b / 255.0f;
        verts[i].color.a = a / 255.0f;
        verts[i].tex_coord.x = 0;
        verts[i].tex_coord.y = 0;
    }
    SDL_RenderGeometry(ren, NULL, verts, 6, NULL, 0);
}

static void draw_diamond(SDL_Renderer *ren, float cx, float cy, float size,
                         Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Vertex verts[6];
    SDL_FPoint pts[4] = {
        { cx, cy - size },
        { cx + size * 0.6f, cy },
        { cx, cy + size },
        { cx - size * 0.6f, cy }
    };
    int idx[] = {0,1,2, 0,2,3};
    for (int i = 0; i < 6; i++) {
        verts[i].position = pts[idx[i]];
        verts[i].color.r = r / 255.0f;
        verts[i].color.g = g / 255.0f;
        verts[i].color.b = b / 255.0f;
        verts[i].color.a = a / 255.0f;
        verts[i].tex_coord.x = 0;
        verts[i].tex_coord.y = 0;
    }
    SDL_RenderGeometry(ren, NULL, verts, 6, NULL, 0);
}

/* Simple digit renderer (7-segment style) using SDL_RenderLine */
static void draw_char(SDL_Renderer *ren, char c, float x, float y, float scale) {
    /*  _ 
     * |_|  segments: top, top-left, top-right, mid, bot-left, bot-right, bot
     * |_|  encoded as bits: 0-6
     */
    static const Uint8 segs[10] = {
        0x77, /* 0: top tl tr    bl br bot */
        0x24, /* 1:       tr       br      */
        0x5D, /* 2: top    tr mid bl    bot */
        0x6D, /* 3: top    tr mid    br bot */
        0x2E, /* 4:     tl tr mid    br     */
        0x6B, /* 5: top tl    mid    br bot */
        0x7B, /* 6: top tl    mid bl br bot */
        0x25, /* 7: top    tr       br      */
        0x7F, /* 8: all                     */
        0x6F, /* 9: top tl tr mid    br bot */
    };
    if (c < '0' || c > '9') return;
    Uint8 s = segs[c - '0'];
    float w = 8 * scale, h = 14 * scale, g = 1 * scale;
    /* top */    if (s & 0x01) SDL_RenderLine(ren, x+g, y,   x+w-g, y);
    /* top-l */  if (s & 0x02) SDL_RenderLine(ren, x, y+g,   x, y+h/2-g);
    /* top-r */  if (s & 0x04) SDL_RenderLine(ren, x+w, y+g, x+w, y+h/2-g);
    /* mid */    if (s & 0x08) SDL_RenderLine(ren, x+g, y+h/2, x+w-g, y+h/2);
    /* bot-l */  if (s & 0x10) SDL_RenderLine(ren, x, y+h/2+g, x, y+h-g);
    /* bot-r */  if (s & 0x20) SDL_RenderLine(ren, x+w, y+h/2+g, x+w, y+h-g);
    /* bot */    if (s & 0x40) SDL_RenderLine(ren, x+g, y+h, x+w-g, y+h);
}

static void draw_number(SDL_Renderer *ren, int num, float x, float y, float scale) {
    char buf[16];
    SDL_snprintf(buf, sizeof(buf), "%d", num);
    float spacing = 12 * scale;
    for (int i = 0; buf[i]; i++)
        draw_char(ren, buf[i], x + i * spacing, y, scale);
}

/* ------------------------------------------------------------------ */
/* Render                                                              */
/* ------------------------------------------------------------------ */
static void game_render(Game *g, SDL_Renderer *ren) {
    /* bg */
    SDL_SetRenderDrawColor(ren, 8, 8, 20, 255);
    SDL_RenderClear(ren);

    /* stars */
    for (int i = 0; i < MAX_STARS; i++) {
        Uint8 b = g->stars[i].brightness;
        SDL_SetRenderDrawColor(ren, b, b, (Uint8)(b + (255 - b) / 2), 255);
        SDL_RenderPoint(ren, g->stars[i].x, g->stars[i].y);
    }

    /* particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (!p->active) continue;
        Uint8 a = (Uint8)(p->life * 255);
        float sz = 2.0f + p->life * 2.0f;
        SDL_FRect pr = { p->x - sz/2, p->y - sz/2, sz, sz };
        SDL_SetRenderDrawColor(ren, p->r, p->g, p->b, a);
        SDL_RenderFillRect(ren, &pr);
    }

    /* rocks */
    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock *r = &g->rocks[i];
        if (!r->active) continue;
        draw_rotated_rect(ren, r->x, r->y, r->w, r->h, r->angle,
                          180, 60, 60, 255);
        draw_rotated_rect(ren, r->x, r->y, r->w * 0.7f, r->h * 0.7f,
                          r->angle + 15, 220, 90, 90, 255);
    }

    /* player */
    if (g->alive) {
        /* glow */
        draw_diamond(ren, g->px, g->py, PLAYER_SIZE * 1.5f, 30, 100, 200, 80);
        /* body */
        draw_diamond(ren, g->px, g->py, PLAYER_SIZE, 80, 180, 255, 255);
        /* core */
        draw_diamond(ren, g->px, g->py, PLAYER_SIZE * 0.5f, 200, 230, 255, 255);
    }

    /* HUD - score */
    SDL_SetRenderDrawColor(ren, 200, 220, 255, 255);
    draw_number(ren, (int)g->score, 20, 20, 2.0f);

    /* high score */
    if (g->high_score > 0) {
        SDL_SetRenderDrawColor(ren, 255, 200, 80, 180);
        draw_number(ren, (int)g->high_score, SCREEN_W - 180, 20, 1.2f);
    }

    /* game over overlay */
    if (!g->alive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
        SDL_FRect overlay = { 0, 0, SCREEN_W, SCREEN_H };
        SDL_RenderFillRect(ren, &overlay);

        /* flashing "press space" hint via simple rect */
        float t = (float)SDL_GetTicks() / 500.0f;
        Uint8 flash = (Uint8)(140 + 100 * sinf(t));
        SDL_SetRenderDrawColor(ren, flash, flash, flash, 255);
        /* small centered bar as "press space" indicator */
        SDL_FRect bar = { SCREEN_W/2 - 60, SCREEN_H/2 + 40, 120, 4 };
        SDL_RenderFillRect(ren, &bar);

        /* show final score large */
        SDL_SetRenderDrawColor(ren, 255, 100, 80, 255);
        draw_number(ren, (int)g->score, SCREEN_W/2 - 50, SCREEN_H/2 - 20, 3.0f);
    }

    SDL_RenderPresent(ren);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_CreateWindowAndRenderer("Neon Dodge - SDL3", SCREEN_W, SCREEN_H, 0,
                                     &window, &renderer)) {
        SDL_Log("CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    srand((unsigned)time(NULL));
    Game game = {0};
    game_reset(&game);

    int running = 1;
    Uint64 last = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = 0;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode k = ev.key.key;
                if (k == SDLK_ESCAPE || k == SDLK_Q) running = 0;
                if (k == SDLK_UP    || k == SDLK_W) game.key_up    = 1;
                if (k == SDLK_DOWN  || k == SDLK_S) game.key_down  = 1;
                if (k == SDLK_LEFT  || k == SDLK_A) game.key_left  = 1;
                if (k == SDLK_RIGHT || k == SDLK_D) game.key_right = 1;
                if (k == SDLK_SPACE && !game.alive) game_reset(&game);
            }
            if (ev.type == SDL_EVENT_KEY_UP) {
                SDL_Keycode k = ev.key.key;
                if (k == SDLK_UP    || k == SDLK_W) game.key_up    = 0;
                if (k == SDLK_DOWN  || k == SDLK_S) game.key_down  = 0;
                if (k == SDLK_LEFT  || k == SDLK_A) game.key_left  = 0;
                if (k == SDLK_RIGHT || k == SDLK_D) game.key_right = 0;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / (float)freq;
        last = now;
        if (dt > 0.05f) dt = 0.05f;   /* clamp large spikes */

        game_update(&game, dt);
        game_render(&game, renderer);

        SDL_Delay(1000 / FPS);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
