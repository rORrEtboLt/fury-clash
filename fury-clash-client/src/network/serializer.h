#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * Minimal serializer interface.
 * Initial implementation uses raw struct copies (little-endian).
 * Replace with FlatBuffers for production to get zero-copy parsing.
 */

typedef struct ByteBuf {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    size_t   pos;   /* read cursor */
} ByteBuf;

void    bbuf_init_write(ByteBuf *b, uint8_t *buf, size_t cap);
void    bbuf_init_read(ByteBuf *b, uint8_t *buf, size_t len);
void    bbuf_write_u8(ByteBuf *b, uint8_t v);
void    bbuf_write_u16(ByteBuf *b, uint16_t v);
void    bbuf_write_u32(ByteBuf *b, uint32_t v);
uint8_t  bbuf_read_u8(ByteBuf *b);
uint16_t bbuf_read_u16(ByteBuf *b);
uint32_t bbuf_read_u32(ByteBuf *b);
