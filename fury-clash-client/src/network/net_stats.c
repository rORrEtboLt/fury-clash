#include "net_stats.h"
#include <string.h>
#include <math.h>

void net_stats_init(NetStats *s) {
    memset(s, 0, sizeof(*s));
}

void net_stats_record_pong(NetStats *s, float rtt_ms) {
    float ping = rtt_ms / 2.0f;
    float diff = ping - s->ping_ms;
    s->jitter_ms = s->jitter_ms * 0.9f + fabsf(diff) * 0.1f;
    s->ping_ms   = s->ping_ms   * 0.9f + ping          * 0.1f;
}

void net_stats_record_rollback(NetStats *s, int frames) {
    s->rollback_frame_count += frames;
}
