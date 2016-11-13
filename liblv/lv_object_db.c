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
#include <string.h>

#include "lv_pack.h"
#include "lv_object_db.h"
#include "buffer.h"

#define OBJECT_ENTRY_SIZE 0x15

int lv_object_db_get_object(struct lv_object_db *db, uint16_t index,
                            struct lv_object_db_entry *entry)
{
    uint16_t chunk_sprites, prog_offset, dummy16;
    uint8_t width, height, dummy8;

    /*
     * Object records:
     *   [00] u16: Sprite chunk (0xffff = none, 0xfffe = ?)
     *   [02]  u8:
     *   [03] u16: Object program offset
     *   [05] u16:
     *   [07] u16:
     *   [09]  u8: Width
     *   [0a]  u8: Height
     *   [0b] u16:
     *   [0d] u16:
     *   [0f] u16:
     *   [11] u16:
     *   [13] u16:
     */
    memset(entry, 0, sizeof(*entry));
    buffer_seek(&db->buf, index * OBJECT_ENTRY_SIZE);

    buffer_get_le16(&db->buf, &chunk_sprites);
    buffer_get_u8(&db->buf, &dummy8);
    buffer_get_le16(&db->buf, &prog_offset);
    buffer_get_le16(&db->buf, &dummy16);
    buffer_get_le16(&db->buf, &dummy16);
    buffer_get_u8(&db->buf, &width);
    buffer_get_u8(&db->buf, &height);
    buffer_get_le16(&db->buf, &dummy16);
    buffer_get_le16(&db->buf, &dummy16);
    buffer_get_le16(&db->buf, &dummy16);
    buffer_get_le16(&db->buf, &dummy16);
    buffer_get_le16(&db->buf, &dummy16);

    entry->chunk_sprites = chunk_sprites;
    entry->prog_offset = prog_offset;
    entry->width = width;
    entry->height = height;

    return 0;
}

int lv_object_db_load(struct lv_pack *pack, struct lv_object_db *db,
		      unsigned chunk_index)
{
    struct lv_chunk *chunk;

    memset(db, 0, sizeof(*db));

    chunk = lv_pack_get_chunk(pack, chunk_index);
    lv_decompress_chunk(chunk, &db->data);
    buffer_init_from_data(&db->buf, db->data, chunk->decompressed_size);

    return 0;
}
