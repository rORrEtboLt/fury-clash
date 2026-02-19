#include "serializer.h"
#include <string.h>

void bbuf_init_write(ByteBuf *b, uint8_t *buf, size_t cap) {
    b->data = buf; b->len = 0; b->cap = cap; b->pos = 0;
}
void bbuf_init_read(ByteBuf *b, uint8_t *buf, size_t len) {
    b->data = buf; b->len = len; b->cap = len; b->pos = 0;
}

void bbuf_write_u8(ByteBuf *b, uint8_t v) {
    if (b->len < b->cap) b->data[b->len++] = v;
}
void bbuf_write_u16(ByteBuf *b, uint16_t v) {
    bbuf_write_u8(b, (uint8_t)(v));
    bbuf_write_u8(b, (uint8_t)(v >> 8));
}
void bbuf_write_u32(ByteBuf *b, uint32_t v) {
    bbuf_write_u16(b, (uint16_t)(v));
    bbuf_write_u16(b, (uint16_t)(v >> 16));
}

uint8_t bbuf_read_u8(ByteBuf *b) {
    return (b->pos < b->len) ? b->data[b->pos++] : 0;
}
uint16_t bbuf_read_u16(ByteBuf *b) {
    uint16_t lo = bbuf_read_u8(b);
    uint16_t hi = bbuf_read_u8(b);
    return lo | (hi << 8);
}
uint32_t bbuf_read_u32(ByteBuf *b) {
    uint32_t lo = bbuf_read_u16(b);
    uint32_t hi = bbuf_read_u16(b);
    return lo | (hi << 16);
}
