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
#include <stdlib.h>
#include <stdio.h>

#include "lv_pack.h"
#include "lv_compress.h"

#include "buffer.h"

/* Flag used by some Blackthorne chunks. Use unknown. */
#define BT_CHUNK_FLAG 0x40000000

int lv_pack_load(const char *filename, struct lv_pack *pack, bool blackthorne)
{
    struct lv_chunk *chunk, *next_chunk;
    struct buffer buf;
    uint32_t num_chunks, chunk_start, val32;
    uint16_t val16;
    int i, err;

    memset(pack, 0, sizeof(*pack));
    pack->blackthorne = blackthorne;

    err = buffer_init_from_file(&buf, filename);
    if (err)
        return err;

    /* Get the number of chunks and allocate an array for them */
    if (pack->blackthorne) {
        /* Blackthorne stores the number of chunks as the first le32 value */
        buffer_get_le32(&buf, &num_chunks);
        pack->num_chunks = num_chunks;
    } else {
        /*
         * Lost Vikings doesn't store the number of chunks in the pack file.
         * The number of chunks can be calculated using the offset for the
         * first chunk since the chunks are ordered.
         */
        buffer_get_le32(&buf, &chunk_start);
        pack->num_chunks = (chunk_start / 4) - 1;
    }

    pack->chunks = calloc(pack->num_chunks, sizeof(*pack->chunks));
    if (!pack->chunks)
        return -1;

    /* Get the starting offset of each chunk */
    buffer_seek(&buf, 0);

    for (i = 0; i < pack->num_chunks; i++) {
        /*
         * Blackthorne doesn't have a chunk zero because of the 4-byte
         * pack header.
         */
        if (blackthorne && i == 0 ) {
            buffer_get_le32(&buf, &val32);
            continue;
        }

        buffer_get_le32(&buf, &pack->chunks[i].start);

        if (pack->blackthorne) {
            /*
             * Some blackthorne chunks have a flag set in the upper bits
             * of the chunk offset. Its use it not yet known. Clear it.
             */
            if (pack->chunks[i].start & BT_CHUNK_FLAG) {
                pack->chunks[i].flag = true;
                pack->chunks[i].start &= ~BT_CHUNK_FLAG;
            }
        }
    }

    /* Get the chunk size and data */
    for (i = 0; i < pack->num_chunks; i++) {
        chunk = &pack->chunks[i];

        if (i < pack->num_chunks - 1) {
            next_chunk = &pack->chunks[i + 1];
            chunk->size = (next_chunk->start & ~BT_CHUNK_FLAG) - chunk->start;
        } else {
            chunk->size = buf.size - chunk->start;
        }

        chunk->index = i;

        /*
         * Chunks store their decompressed size at the beginning of the chunk
         * data. The Lost Vikings stores the size as a 16-bit value,
         * Blackthorne stores it as a 32-bit value.
         */
        if (pack->blackthorne) {
            buffer_peek_le32(&buf, chunk->start, &val32);
            chunk->decompressed_size = val32;
            chunk->data_offset = 4;
        } else {
            buffer_peek_le16(&buf, chunk->start, &val16);
            chunk->decompressed_size = val16 + 1;
            chunk->data_offset = 2;
        }


        chunk->data = malloc(chunk->size);
        if (!chunk->data)
            return -1;

        memcpy(chunk->data, buf.data + chunk->start, chunk->size);
    }

    return 0;
}

struct lv_chunk *lv_pack_get_chunk(struct lv_pack *pack, unsigned chunk_index)
{
    if (chunk_index >= pack->num_chunks)
        return NULL;
    return &pack->chunks[chunk_index];
}

int lv_decompress_chunk(struct lv_chunk *chunk, uint8_t **dst)
{
    *dst = malloc(chunk->decompressed_size);
    if (!(*dst))
        return -1;

    /* Skip the header in the chunk */
    lv_decompress(chunk->data + chunk->data_offset,
                  chunk->size - chunk->data_offset,
                  *dst, chunk->decompressed_size);
    return 0;
}
