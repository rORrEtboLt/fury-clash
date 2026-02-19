#include "audio.h"
#include "sfx.h"
#include "music.h"
#include <SDL3/SDL.h>

static SDL_AudioDeviceID g_device = 0;

int audio_init(void) {
    SDL_AudioSpec spec = {
        .format    = SDL_AUDIO_S16LE,
        .channels  = 2,
        .freq      = 44100
    };
    g_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!g_device) {
        SDL_Log("[audio] SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return 0;
    }
    sfx_init(g_device);
    music_init(g_device);
    return 1;
}

void audio_shutdown(void) {
    music_shutdown();
    sfx_shutdown();
    if (g_device) { SDL_CloseAudioDevice(g_device); g_device = 0; }
}

void audio_update(void)               { music_update(); }
void audio_play_sfx(int sfx_id)       { sfx_play(sfx_id); }
void audio_set_sfx_volume(float v)    { sfx_set_volume(v); }
void audio_set_music_volume(float v)  { music_set_volume(v); }
void audio_play_music(int id, int lp) { music_play(id, lp); }
void audio_stop_music(void)           { music_stop(); }
