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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lv_compress.h"
#include "common.h"

#define RLE_MIN_LENGTH	3
#define RLE_MAX_LENGTH	18

static uint8_t rle_table_read(const uint8_t *table, unsigned *index)
{
    uint8_t byte;

    byte = table[*index];
    (*index)++;
    (*index) &= 0xfff;

    return byte;
}

static bool rle_table_find(const uint8_t *table, unsigned table_index,
                           const uint8_t *data, size_t data_size,
                           unsigned *index, size_t *length)
{
    int i, seq_len, offset;

    for (i = 0; i < 0x1000; i++) {
        for (seq_len = 0; seq_len < data_size; seq_len++) {
            offset = i + seq_len;
            if (table[offset % 0xfff] != data[seq_len])
                break;

            /*
             * Don't return RLE runs that are in the area of the table that
             * will be written to, since the values will change.
             */
            if ((i >= table_index && i <= table_index + seq_len) ||
                (offset >= table_index && offset <= table_index + seq_len)) {
                seq_len = 0;
                break;
            }
        }
        if (seq_len >= RLE_MIN_LENGTH) {
            *index = i;
            *length = seq_len;
            return true;
        }
    }

    return false;
}

static void rle_table_write(uint8_t *table, unsigned *index, uint8_t byte)
{
    table[*index] = byte;
    (*index)++;
    (*index) &= 0xfff;
}

void lv_decompress(const uint8_t *src, size_t src_size,
		   uint8_t *dst, size_t dst_size)
{
    unsigned bit, i, count, src_offset = 0, dst_offset = 0, table_index = 0,
        rle_index;
    uint8_t table[0x1000] = {0};
    uint16_t word;
    uint8_t ctrl_byte, byte;

    while (dst_offset < dst_size) {
        ctrl_byte = src[src_offset++];
        for (bit = 0; bit < 8; bit++) {
            if (dst_offset >= dst_size)
                break;

            if (ctrl_byte & (1 << bit)) {
                /* Output the next byte and store it in the table */
                byte = src[src_offset++];

                dst[dst_offset++] = byte;
                rle_table_write(table, &table_index, byte);

            } else {
                /*
                 *
                 */
                word = (src[src_offset + 0] << 0) | (src[src_offset + 1] << 8);
                src_offset += 2;

                count = ((word & 0xf000) >> 12) + 3;
                rle_index = word & 0xfff;

                for (i = 0; i < count; i++) {
                    byte = rle_table_read(table, &rle_index);
                    rle_table_write(table, &table_index, byte);
                    dst[dst_offset++] = byte;
                }
            }
        }
    }
}

size_t lv_compress(const uint8_t *src, size_t src_size,
		   uint8_t *dst, size_t dst_size)
{
    uint8_t table[0x1000] = {0}, byte;
    unsigned i, bit, src_offset = 0, dst_offset = 0, table_index = 0,
        rle_index, ctrl_byte_offset;
    size_t rle_len;
    uint16_t word;
    bool found;

    while (src_offset < src_size) {
        /* Write a place-holder control byte */
        ctrl_byte_offset = dst_offset++;
        dst[ctrl_byte_offset] = 0;

        for (bit = 0; bit < 8; bit++) {
            if (src_offset >= src_size)
                break;

            /* Try to find an repeated sequence */
            found = rle_table_find(table, table_index, &src[src_offset],
                                   min(src_size - src_offset, RLE_MAX_LENGTH),
                                   &rle_index, &rle_len);
            if (found) {
                /*
                 * RLE sequence found. Encode a 16-bit word for length
                 * and index. Control byte flag is not set.
                 */
                word  = ((rle_len - 3) & 0xf) << 12;
                word |= rle_index & 0xfff;

                dst[dst_offset++] = word;
                dst[dst_offset++] = word >> 8;

                for (i = 0; i < rle_len; i++)
                    rle_table_write(table, &table_index, src[src_offset++]);

            } else {
                /*
                 * No sequence found. Encode the byte directly.
                 * Control byte flag is set.
                 */
                dst[ctrl_byte_offset] |= (1 << bit);
                byte = src[src_offset++];
                dst[dst_offset++] = byte;
                rle_table_write(table, &table_index, byte);
            }
        }
    }

    return dst_offset;
}
