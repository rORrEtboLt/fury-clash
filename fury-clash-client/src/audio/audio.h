#pragma once
#include <SDL3/SDL.h>

int  audio_init(void);
void audio_shutdown(void);
void audio_update(void);   /* advance music streaming */

void audio_play_sfx(int sfx_id);
void audio_set_sfx_volume(float v);    /* 0.0 - 1.0 */
void audio_set_music_volume(float v);
void audio_play_music(int music_id, int loop);
void audio_stop_music(void);

/* SFX IDs */
#define SFX_HIT_LIGHT  0
#define SFX_HIT_HEAVY  1
#define SFX_BLOCK      2
#define SFX_KO         3
#define SFX_MENU_SELECT 4

/* Music IDs */
#define MUSIC_MENU    0
#define MUSIC_FIGHT   1
#define MUSIC_RESULTS 2
