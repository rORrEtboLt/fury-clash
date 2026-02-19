#pragma once

typedef struct NetStats {
    float ping_ms;
    float jitter_ms;
    int   rollback_frame_count;
    int   packets_lost;
} NetStats;

void net_stats_init(NetStats *s);
void net_stats_record_pong(NetStats *s, float rtt_ms);
void net_stats_record_rollback(NetStats *s, int frames);
