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

#include <stdint.h>
#include <stdio.h>

#include "lv_sprite.h"
#include "common.h"

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

void lv_sprite_draw_packed32(const uint8_t *sprite, uint8_t base_color,
			     bool flip, uint8_t *dst, unsigned dst_x,
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
                    if (flip)
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
