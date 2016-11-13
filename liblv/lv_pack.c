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
#include <stdlib.h>
#include <stdio.h>

#include "lv_pack.h"
#include "lv_compress.h"

#include "buffer.h"

int lv_pack_load(const char *filename, struct lv_pack *pack)
{
    struct lv_chunk *chunk, *next_chunk;
    struct buffer buf;
    uint32_t chunk_start;
    uint16_t val;
    int i, err;

    memset(pack, 0, sizeof(*pack));

    err = buffer_init_from_file(&buf, filename);
    if (err)
        return err;

    /*
     * Get the number of chunks and allocate an array for them.
     * One extra chunk is allocated to get the size of the last chunk.
     */
    buffer_get_le32(&buf, &chunk_start);
    pack->num_chunks = (chunk_start / 4) - 1;
    pack->chunks = calloc(pack->num_chunks + 1, sizeof(*pack->chunks));
    if (!pack->chunks)
        return -1;

    /* Get the start offset of each chunk */
    pack->chunks[0].start = chunk_start;
    for (i = 1; i <= pack->num_chunks; i++)
        buffer_get_le32(&buf, &pack->chunks[i].start);

    /* Get the chunk size and data */
    for (i = 0; i < pack->num_chunks; i++) {
        chunk = &pack->chunks[i];
        next_chunk = &pack->chunks[i + 1];

        chunk->index = i;

        buffer_peek_le16(&buf, chunk->start, &val);

        chunk->size = next_chunk->start - chunk->start;
        chunk->decompressed_size = val + 1;

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

    /* Skip the 2-byte size header in the chunk */
    lv_decompress(chunk->data + 2, chunk->size - 2,
                  *dst, chunk->decompressed_size);
    return 0;
}
