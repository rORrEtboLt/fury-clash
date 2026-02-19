#include "input_buffer.h"
#include <string.h>

void ibuf_init(InputBuffer *ib, int start_frame) {
    memset(ib, 0, sizeof(*ib));
    ib->base_frame = start_frame;
}

void ibuf_put(InputBuffer *ib, int frame, uint16_t input) {
    int slot = (frame - ib->base_frame) % INPUT_BUF_SIZE;
    if (slot < 0) return;
    ib->inputs[slot]    = input;
    ib->confirmed[slot] = 1;
}

uint16_t ibuf_get(const InputBuffer *ib, int frame) {
    int slot = (frame - ib->base_frame) % INPUT_BUF_SIZE;
    if (slot < 0) return 0;
    return ib->inputs[slot];
}

int ibuf_is_confirmed(const InputBuffer *ib, int frame) {
    int slot = (frame - ib->base_frame) % INPUT_BUF_SIZE;
    if (slot < 0) return 0;
    return ib->confirmed[slot];
}
