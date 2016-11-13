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

#ifndef _LV_PACK_H
#define _LV_PACK_H

#include <stdint.h>
#include <stdio.h>

/**
 * \defgroup lv_pack Pack file
 * \{
 *
 * The Lost Vikings uses a single data file called DATA.DAT. This file is
 * an archive containing multiple chunks, most of which are compressed using
 * LZSS compression.
 */

/** Representation of a single data file chunk. */
struct lv_chunk {
    /** Chunk index. */
    unsigned         index;

    /** Chunk data as it appears in the data file. It may be compressed. */
    void             *data;

    /** Start offset of the chunk in the data file. */
    uint32_t         start;

    /** Size of the chunk data as it appears in the data file. */
    size_t           size;

    /**
     * Decompressed size of the chunk. For non-compressed chunks this value
     * is not used.
     */
    size_t           decompressed_size;
};

/** Representation of the The Lost Vikings DATA.DAT pack file. */
struct lv_pack {
    /** Array of chunks. */
    struct lv_chunk  *chunks;

    /** Number of chunks. */
    size_t           num_chunks;
};

/**
 * Load a Lost Vikings pack file (DATA.DAT).
 *
 * \param filename  Filename.
 * \param pack      Pack file structure.
 * \returns         0 for success.
 */
int lv_pack_load(const char *filename, struct lv_pack *pack);

/**
 * Get a chunk from a pack file.
 *
 * \param pack        Pack file.
 * \param chunk_index Index of the chunk to get.
 * \returns           Chunk or NULL if not found.
 */
struct lv_chunk *lv_pack_get_chunk(struct lv_pack *pack, unsigned chunk_index);

/**
 * Decompress the data for a chunk.
 *
 * \param chunk   Chunk to decompress.
 * \param dst     Destination buffer to decompress into. This buffer will
 *                be allocated automatically. The caller must free it.
 * \returns       0 for success.
 */
int lv_decompress_chunk(struct lv_chunk *chunk, uint8_t **dst);

/** \} */

#endif /* _LV_PACK_H */
