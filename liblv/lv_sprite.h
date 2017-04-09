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

#ifndef _LV_SPRITE_H
#define _LV_SPRITE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * \defgroup lv_sprite Sprites
 * \{
 *
 * Lost Vikings uses VGA 320x240 Mode-X planar graphics. There are multiple
 * sprite formats:
 *
 *  - Raw: Sprite data is stored in raw planar form and can be blitted
 *    directly to the screen. This format does not support transparency.
 *
 *  - Unpacked: Sprite data is stored in an unpacked format with a
 *    transparency mask preceeding each set of 8 pixels. The format is
 *    unpacked because the transparent pixels are stored as zeros in
 *    the data chunk.
 *
 *  - Packed: Sprite data is stored in a packed format with a transparency
 *    mask preceeding each set of 8 pixels. Packed sprites are always 32x32
 *    pixels and are are limited to 16 colors (each byte stores 2 pixels).
 *    Transparent pixels are not stored, so this format provides some
 *    compression for sprites.
 */

/**
 * Draw a raw planar sprite onto a linear 8-bit surface. Raw sprites do not
 * have transparency.
 *
 * \param sprite        Sprite data.
 * \param sprite_width  Sprite width.
 * \param sprite_height Sprite height.
 * \param flip_horiz    Draw horizontally flipped.
 * \param flip_vert     Draw vertically flipped.
 * \param dst           Destination 8-bit surface.
 * \param dst_x         X offset to draw at on the destination surface.
 * \param dst_y         Y offset to draw at on the destination surface.
 * \param dst_width     Width of the destination surface.
 */
void lv_sprite_draw_raw(const uint8_t *sprite, size_t sprite_width,
                        size_t sprite_height, bool flip_horiz, bool flip_vert,
			uint8_t *dst, unsigned dst_x, unsigned dst_y,
			size_t dst_width);

/**
 * Draw a packed 32x32 planar sprite onto a linear 8-bit surface. Packed
 * sprite are always 32x32 pixels and 16 colors. Each set of eight pixels
 * is encoded starting with an 8-bit mask of pixels to be draw, followed by
 * the non-masked pixels stored as 4-bit values. If an odd number of pixels
 * are specified in the mask, then the final nibble is zero. Transparent
 * pixels are not drawn.
 *
 * \param sprite        Sprite data.
 * \param base_color    Base color to add to each pixel value.
 * \param flip          Set to draw the sprite flipped horizontally.
 * \param dst           Destination 8-bit surface.
 * \param dst_x         X offset to draw at on the destination surface.
 * \param dst_y         Y offset to draw at on the destination surface.
 * \param dst_width     Width of the destination surface.
 */
void lv_sprite_draw_packed32(const uint8_t *sprite, uint8_t base_color,
                             bool flip, uint8_t *dst, unsigned dst_x,
                             unsigned dst_y, size_t dst_width);

/**
 * Draw an unpacked format sprite. Unpacked sprites can be any multiple of
 * 8 size. Each set of 8 pixels is preceeded by an 8-bit mask value. Masked
 * (transparent) pixels are stored as zeros. Unpacked sprites pixel values
 * may use the full 256 color range. Transparent pixels are not drawn.
 *
 * \param sprite        Sprite data.
 * \param sprite_width  Sprite width.
 * \param sprite_height Sprite height.
 * \param flip          Set to draw the sprite flipped horizontally.
 * \param dst           Destination 8-bit surface.
 * \param dst_x         X offset to draw at on the destination surface.
 * \param dst_y         Y offset to draw at on the destination surface.
 * \param dst_width     Width of the destination surface.
 */
void lv_sprite_draw_unpacked(const uint8_t *sprite,
                             size_t sprite_width, size_t sprite_height,
                             bool flip, uint8_t *dst,
                             unsigned dst_x, unsigned dst_y, size_t dst_width);

/**
 * Get the size of the data required to stored an unpacked sprite. Unpacked
 * sprites use 9 bytes per pixel: 1 mask byte, followed by 8 pixels.
 * Transparent pixels (masked) are stored as zero bytes.
 *
 * \param width  Sprite width.
 * \param height Sprite height.
 * \returns      Data size of a single sprite.
 */
static inline size_t lv_sprite_unpacked_data_size(size_t width, size_t height)
{
    return ((width * height) / 8) * 9;
}

/* \} */

#endif /* _LV_SPRITE */
