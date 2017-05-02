/*
 * This file is part of The Lost Vikings Library/Tools
 *
 * Ryan Mallon, 2016, <rmallon@gmail.com>
 *
 * To the extent possible under law, the author(s) have dedicated all
 * copyright and related and neighboring rights to this software to
 * the public domain worldwide. This software is distributed without
 * any warranty.  You should have received a copy of the CC0 Public
 * Domain Dedication along with this software. If not, see
 *
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "lv_sprite.h"
#include "lv_pack.h"
#include "common.h"
#include "buffer.h"

typedef void (*lv_sprite_draw_func_t)(const uint8_t *sprite, uint8_t base_color,
                                      size_t sprite_width, size_t sprite_height,
                                      bool flip_horiz, bool flip_vert,
                                      uint8_t *dst, unsigned dst_x,
                                      unsigned dst_y, size_t dst_width);

struct sprite_part {
    unsigned x;
    unsigned y;
    size_t   width;
    size_t   height;
};

struct sprite_layout {
    struct sprite_part parts[16];
    size_t             num_parts;
};

static const struct sprite_layout blackthorne_32x48_layout = {
    .parts = {
        { 0,  0, 16, 16},
        {16,  0, 16, 16},
        { 0, 16, 32, 32},
    },
    .num_parts = 3,
};

static const struct sprite_layout blackthorne_48x48_layout = {
    .parts = {
        { 0,  0, 16, 16},
        {16,  0, 16, 16},
        {32,  0, 16, 16},
        { 0, 16, 32, 32},
        {32, 16, 16, 16},
        {32, 32, 16, 16},
    },
    .num_parts = 6,
};

static const struct sprite_layout blackthorne_48x64_layout = {
    .parts = {
        { 0,  0, 32, 32},
        {32,  0, 16, 16},
        {32, 16, 16, 16},
        { 0, 32, 32, 32},
        {32, 32, 16, 16},
        {32, 48, 16, 16},
    },
    .num_parts = 6,
};

static const struct sprite_layout *get_sprite_layout(size_t width,
                                                     size_t height)
{
    if (width == 32 && height == 48)
        return &blackthorne_32x48_layout;
    if (width == 48 && height == 48)
        return &blackthorne_48x48_layout;
    if (width == 48 && height == 64)
        return &blackthorne_48x64_layout;
    return NULL;
}

#define PACKED_SPRITE_WIDTH  32
#define PACKED_SPRITE_HEIGHT 32

void lv_sprite_draw_raw(const uint8_t *sprite, uint8_t base_color,
			size_t sprite_width, size_t sprite_height,
			bool flip_horiz, bool flip_vert, uint8_t *dst,
			unsigned dst_x, unsigned dst_y, size_t dst_width)
{
    unsigned plane, i, x, y, offset;
    uint8_t pixel;

    for (plane = 0; plane < 4; plane++) {
        for (i = 0; i < (sprite_width * sprite_height) / 4; i++) {
            offset = (i * 4) + plane;
            y = offset / sprite_width;
            x = offset % sprite_width;

            if (flip_horiz)
                x = sprite_width - x;
            if (flip_vert)
                y = sprite_height - y;

	    pixel = (*sprite++) + base_color;
            dst[((dst_y + y) * dst_width) + (dst_x + x)] = pixel;
        }
    }
}

static void __lv_sprite_draw_packed32(const uint8_t *sprite, uint8_t base_color,
                                      size_t sprite_width, size_t sprite_height,
                                      bool flip_horiz, bool flip_vert,
                                      uint8_t *dst, unsigned dst_x,
                                      unsigned dst_y, size_t dst_width)
{
    int num_pixels, plane, x, y, bit;
    uint8_t mask, pixel;

    for (plane = 0; plane < 4; plane++) {
        for (y = 0; y < PACKED_SPRITE_HEIGHT; y++) {
            /*
             * Draw a single horizontal line for a plane, which is a maximum
             * of 8 pixels. Each line starts with a u8 mask value which
             * specifies which pixels are drawn for the current plane,
             * followed by a set of 4-bit values (and one pad value if the
             * number of pixels is odd) for the pixels.
             */
            num_pixels = 0;
            mask = *sprite++;
            for (bit = 7; bit >= 0; bit--) {
                if (mask & (1 << bit)) {
                    pixel = *sprite;
                    if (num_pixels & 1)
                        sprite++;
                    else
                        pixel >>= 4;
                    pixel &= 0xf;
                    pixel += base_color;

                    x = ((7 - bit) * 4) + plane;
                    if (flip_horiz)
                        x = PACKED_SPRITE_WIDTH - x;

                    dst[((dst_y + y) * dst_width) + (dst_x + x)] = pixel;

                    num_pixels++;
                }
            }
            if (num_pixels & 1)
                sprite++;
        }
    }
}

void lv_sprite_draw_packed32(const uint8_t *sprite, uint8_t base_color,
                             bool flip, uint8_t *dst, unsigned dst_x,
                             unsigned dst_y, size_t dst_width)
{
    __lv_sprite_draw_packed32(sprite, base_color, 32, 32, flip, false,
                              dst, dst_x, dst_y, dst_width);
}


void lv_sprite_draw_unpacked(const uint8_t *sprite, uint8_t base_color,
			     size_t sprite_width, size_t sprite_height,
			     bool flip_horiz, bool flip_vert, uint8_t *dst,
			     unsigned dst_x, unsigned dst_y, size_t dst_width)
{
    int plane, i, x, y, rx, ry, bit;
    uint8_t mask, pixel;

    x = 0;
    for (plane = 0; plane < 4; plane++) {
        y = 0;
        x = plane;
        for (i = 0; i < (sprite_width * sprite_height) / 4 / 8; i++) {
            mask = *sprite++;
            for (bit = 7; bit >= 0; bit--) {
                pixel = (*sprite++) + base_color;

                if (flip_horiz)
                    rx = sprite_width - x;
                else
                    rx = x;
                if (flip_vert)
                    ry = sprite_height - y;
                else
                    ry = y;

                if (mask & (1 << bit))
                    dst[((dst_y + ry) * dst_width) + (dst_x + rx)] = pixel;

                x += 4;
                if (x >= sprite_width) {
                    y++;
                    x = plane;
                }
            }
        }
    }
}

static const lv_sprite_draw_func_t draw_funcs[] = {
    [LV_SPRITE_FORMAT_RAW]      = lv_sprite_draw_raw,
    [LV_SPRITE_FORMAT_UNPACKED] = lv_sprite_draw_unpacked,
    [LV_SPRITE_FORMAT_PACKED32] = __lv_sprite_draw_packed32,
};

void lv_sprite_draw(const uint8_t *sprite, size_t width, size_t height,
                    unsigned format, uint8_t base_color,
                    bool flip_horiz, bool flip_vert,
                    uint8_t *dst, unsigned dst_x, unsigned dst_y,
                    size_t dst_width)
{
    const struct sprite_layout *layout;
    lv_sprite_draw_func_t draw_func;
    unsigned offset;
    int i;

    draw_func = draw_funcs[format];
    layout = get_sprite_layout(width, height);
    if (layout) {
        /* Multipart sprite */
        for (i = 0, offset = 0; i < layout->num_parts; i++) {
            draw_func(&sprite[offset], base_color,
                      layout->parts[i].width, layout->parts[i].height,
                      flip_horiz, flip_vert, dst,
                      dst_x + layout->parts[i].x, dst_y + layout->parts[i].y,
                      dst_width);

            offset += layout->parts[i].width * layout->parts[i].height;
        }

    } else {
        /* Normal sprite */
        draw_func(sprite, base_color, width, height,
                  flip_horiz, flip_vert, dst, dst_x, dst_y, dst_width);
    }
}

void lv_sprite_load_set(struct lv_sprite_set *set, unsigned format,
                        size_t sprite_width, size_t sprite_height,
                        struct lv_chunk *chunk)
{
    size_t sprite_data_size;
    struct buffer buf;
    uint16_t first_offset, offset;
    int i;

    memset(set, 0, sizeof(*set));
    set->chunk_index = chunk->index;
    set->format = format;

    switch (format) {
    case LV_SPRITE_FORMAT_RAW:
    case LV_SPRITE_FORMAT_UNPACKED:
        sprite_data_size = lv_sprite_data_size(format, sprite_width,
                                               sprite_height);
        set->num_sprites = chunk->decompressed_size / sprite_data_size;

        lv_decompress_chunk(chunk, &set->planar_data);
        set->data_size = chunk->decompressed_size;

        set->sprites = calloc(set->num_sprites, sizeof(uint8_t *));
        for (i = 0; i < set->num_sprites; i++)
            set->sprites[i] = set->planar_data + (sprite_data_size * i);

        break;

    case LV_SPRITE_FORMAT_PACKED32:
        /*
         * Packed 32x32 sprites are variable length. There is a header with
         * a list of offsets in the sprite data chunk.
         *
         * Sprite width and height are ignored for this format.
         */
        lv_decompress_chunk(chunk, &set->planar_data);
        set->data_size = chunk->decompressed_size;

        buffer_init_from_data(&buf, set->planar_data, set->data_size);
        set->num_sprites = 0;

        /* First pass to count */
        buffer_peek_le16(&buf, 0, &first_offset);
        while (1) {
            buffer_get_le16(&buf, &offset);
            if (buffer_offset(&buf) >= first_offset)
                break;

            set->num_sprites++;
        }

        /* Second pass to load the sprite offsets */
        set->sprites = calloc(set->num_sprites, sizeof(uint8_t *));
        buffer_seek(&buf, 0);
        for (i = 0; i < set->num_sprites; i++) {
            buffer_get_le16(&buf, &offset);
            set->sprites[i] = set->planar_data + offset;
        }
        break;
    }
}
