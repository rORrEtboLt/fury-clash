#pragma once
#include <stdint.h>

/* Ring buffer of remote inputs received from network */
#define INPUT_BUF_SIZE 256

typedef struct InputBuffer {
    uint16_t inputs[INPUT_BUF_SIZE];
    int      confirmed[INPUT_BUF_SIZE];
    int      base_frame;  /* frame index of slot 0 */
} InputBuffer;

void     ibuf_init(InputBuffer *ib, int start_frame);
void     ibuf_put(InputBuffer *ib, int frame, uint16_t input);
uint16_t ibuf_get(const InputBuffer *ib, int frame);
int      ibuf_is_confirmed(const InputBuffer *ib, int frame);
