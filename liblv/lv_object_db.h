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

#ifndef _LV_OBJECT_DB_H
#define _LV_OBJECT_DB_H

#include <stdint.h>

#include "buffer.h"

/**
 * \defgroup lv_object_db Object database
 *
 * The object database provides template classes for each of the game objects.
 * Object database chunks are shared across each level in a world.
 *
 * Object database entries have a virtual machine program associated with
 * them that appears to control the behaviour of the object. The virtual
 * machine uses variable length instructions. The virtual machine has not
 * yet been fully reversed.
 */

struct lv_pack;

/** Object database entry. */
struct lv_object_db {
    /** Pointer to the decompressed chunk data. */
    uint8_t        *data;

    /** Accessor buffer for the decompressed chunk data. */
    struct buffer  buf;
};

struct lv_object_db_entry {
    /**
     * Index of the sprites chunk if the object uses unpacked format sprites.
     */
    unsigned       chunk_sprites;

    /** Offset in the chunk of this entry's virtual machine program. */
    unsigned       prog_offset;

    /** Width of the object. */
    unsigned       width;

    /** Height of the object. */
    unsigned       height;
};

/* Special values for chunk_sprites field */
#define LV_OBJECT_DB_SPRITES_NONE    0xffff
#define LV_OBJECT_DB_SPRITES_PACKED  0xfffe

/**
 * Load an object database.
 *
 * \param pack         Pack file.
 * \param db           Database to load.
 * \param chunk_index  Chunk index of the database.
 * \returns            0 for success.
 */
int lv_object_db_load(struct lv_pack *pack, struct lv_object_db *db,
		      unsigned chunk_index);

/**
 * Get an object entry from the database.
 *
 * \param db          Object database.
 * \param index       Object index.
 * \param entry       Returned object entry.
 * \returns           0 for success.
 */
int lv_object_db_get_object(struct lv_object_db *db, uint16_t index,
                            struct lv_object_db_entry *entry);

/** \} */

#endif /* _LV_OBJECT_DB_H */
