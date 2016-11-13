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
#include <string.h>
#include <stdio.h>

#include "lv_level.h"
#include "lv_sprite.h"
#include "lv_pack.h"
#include "lv_debug.h"

#include "buffer.h"
#include "common.h"

/* These are hardcoded in VIKINGS.EXE */
static const struct lv_level_info level_info[] = {
    /* World 1 - Spaceship */
    {198, 449}, {200, 449}, {202, 449}, {204, 449},

    /* World 2 - Caves */
    { 40, 450}, { 42, 450}, { 44, 450}, { 46, 450},
    { 48, 450}, { 50, 450}, { 52, 450},

    /* World 3 - Egypt */
    { 75, 451}, { 77, 451}, { 79, 451}, { 81, 451},
    { 83, 451}, { 85, 451},

    /* World 4 - Construction */
    {114, 452}, {116, 452}, {118, 452}, {120, 452},
    {122, 452}, {124, 452}, {126, 452}, {128, 452},

    /* World 5 - Candy */
    {156, 453}, {158, 453}, {160, 453}, {162, 453},
    {164, 453}, {166, 453}, {168, 453}, {170, 453},

    /* World 6 - Spaceship */
    {206, 449}, {208, 449}, {210, 449}, {212, 449},

    /* Special level? */
    {369, 0xffff},

    /* Special levels */
    {381, 454},
    {390, 454},
    {396, 454}, /* Silicon & Synapse logo */
    {402, 454}, /* Timewarp */
    {382, 454},
    {431, 454}, /* Vikings home (intro) */
    {432, 454}, /* Vikings home (demo) */
    {433, 454},
    {434, 454}, /* Vikings home (ending?) */
    {218, 454}, /* Viking ship ending */
};

const struct lv_level_info *lv_level_get_info(unsigned level_num)
{
    if (level_num < 1 || level_num >= ARRAY_SIZE(level_info))
        return NULL;

    return &level_info[level_num - 1];
}

static int add_object(struct lv_level *level, unsigned type,
                      unsigned xoff, unsigned yoff,
                      unsigned width, unsigned height,
                      unsigned flags, unsigned arg)
{
    struct lv_object *obj;

    obj = &level->objects[level->num_objects];
    obj->type   = type;
    obj->xoff   = xoff;
    obj->yoff   = yoff;
    obj->width  = width;
    obj->height = height;
    obj->flags  = flags;
    obj->arg    = arg;

    level->num_objects++;
    return 0;
}

static int add_viking(struct lv_level *level, unsigned type,
                      unsigned xoff, unsigned yoff, unsigned flags)
{
    // FIXME - height 36 seems wrong, but lines up correctly?
    return add_object(level, type, xoff, yoff, 32, 36, flags, 0);
}

static int load_map(struct lv_pack *pack, struct lv_level *level,
                    unsigned chunk_index)
{
    struct lv_chunk *chunk;
    uint8_t *data;
    size_t map_size;

    map_size = level->width * level->height * sizeof(uint16_t);

    chunk = lv_pack_get_chunk(pack, chunk_index);
    if (chunk->decompressed_size != map_size)
        return -1;

    lv_decompress_chunk(chunk, &data);
    level->map = (uint16_t *)data;

    return 0;
}

static int load_tile_prefabs(struct lv_pack *pack, struct lv_level *level,
                             unsigned chunk_index)
{
    struct lv_chunk *chunk;
    struct buffer buf;
    uint8_t *data;
    uint16_t val;
    int i, j, base;

    chunk = lv_pack_get_chunk(pack, chunk_index);
    lv_decompress_chunk(chunk, &data);
    buffer_init_from_data(&buf, data, chunk->decompressed_size);

    /*
     * Each prefab is 8 bytes long:
     *
     *   [00] Upper left tile
     *   [02] Upper right tile
     *   [04] Lower left tile
     *   [08] Lower right tile
     */
    level->num_prefabs = chunk->decompressed_size / 8;
    level->prefabs = calloc(level->num_prefabs, sizeof(*level->prefabs));

    for (i = 0; i < level->num_prefabs; i++) {
        for (j = 0; j < 4; j++) {
            buffer_get_le16(&buf, &val);

            /* Bits 7..6 the base for the tile index */
            base = (val >> 6) & 0x3;

            /* Upper byte is the tile index */
            level->prefabs[i].tile[j] = base + ((val >> 8) * 4);

            /* Lower 6-bits are flags */
            level->prefabs[i].flags[j] = val & 0x3f;
        }
    }

    free(data);
    return 0;
}

static int load_header(struct lv_pack *pack, struct lv_level *level,
                       struct buffer *buf)
{
    uint16_t width, height, chunk_map, chunk_tileset, chunk_prefabs,
        vikings_xoff, vikings_yoff, vikings_flags, dummy_16;
    uint8_t start_pos_selector, dummy;

    /*
     *   [00] u16: ???
     *   [02] u16: ???
     *   [04] u16: Music track chunk?
     *   [06]  u8: Music type?
     *   [07]  u8: Viking start position selector
     *   [08] u16: Vikings xoff   (selector=0x10)
     *   [0a] u16: Vikings yoff   (selector=0x10)
     *   [0c] u16:
     *   [0e] u16: Vikings flags
     *   [10] u16: Vikings arg    (selector=0x10)
     *
     *   [18]: u8: Selector? (0-2)
     *
     *   [1c]: u8 - special == 0x42?
     *
     *   [29] u16: Level width
     *   [2b] u16: Level height
     *   [2c]  u8: Unknown
     *   [2d] u16: Level map chunk index
     *   [2f] u16: Level tileset chunk index
     *   [31] u16: Level prefabs chunk index
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading header:");
    buffer_seek(buf, 0x07);
    buffer_get_u8(buf, &start_pos_selector);
    buffer_get_le16(buf, &vikings_xoff);
    buffer_get_le16(buf, &vikings_yoff);
    buffer_get_le16(buf, &dummy_16);
    buffer_get_le16(buf, &vikings_flags);

    buffer_seek(buf, 0x29);
    buffer_get_le16(buf, &width);
    buffer_get_le16(buf, &height);
    buffer_get_u8(buf, &dummy);
    buffer_get_le16(buf, &chunk_map);
    buffer_get_le16(buf, &chunk_tileset);
    buffer_get_le16(buf, &chunk_prefabs);

    // FIXME
    buffer_seek(buf, 0x43);

    level->width         = width;
    level->height        = height;
    level->chunk_tileset = chunk_tileset;

    lv_debug(LV_DEBUG_LEVEL, "  Width:         %d", width);
    lv_debug(LV_DEBUG_LEVEL, "  Height:        %d", width);
    lv_debug(LV_DEBUG_LEVEL, "  Start pos sel: %.2x", start_pos_selector);
    lv_debug(LV_DEBUG_LEVEL, "  Chunk map:     %.4x", chunk_map);
    lv_debug(LV_DEBUG_LEVEL, "  Chunk tileset: %.4x", chunk_tileset);
    lv_debug(LV_DEBUG_LEVEL, "  Chunk prefabs: %.4x", chunk_prefabs);

    load_map(pack, level, chunk_map);
    load_tile_prefabs(pack, level, chunk_prefabs);

    /*
     * The start position selector either selects from a bunch of
     * hardcoded positions from VIKINGS.EXE or specifies the position
     * for Erik in the level header and offsets the other two Vikings
     * to his left or right depending on his direction flag.
     *
     * Unfortunately this means you cannot have arbitrary start positions
     * for the Vikings where they do not start together.
     */
    switch (start_pos_selector) {
    case 0x02:
        add_viking(level, LV_OBJ_ERIK, 32, 418, vikings_flags);
        add_viking(level, LV_OBJ_BALEOG, 432, 162,
                   vikings_flags | LV_OBJ_FLAG_FLIP_HORIZ);
        add_viking(level, LV_OBJ_OLAF, 32, 98, vikings_flags);
        break;

    case 0x04:
        add_viking(level, LV_OBJ_ERIK, 39, 322, vikings_flags);
        add_viking(level, LV_OBJ_BALEOG, 191, 288,
                   vikings_flags | LV_OBJ_FLAG_FLIP_HORIZ);
        add_viking(level, LV_OBJ_OLAF, 351, 303, vikings_flags);
        break;

    case 0x05:
        add_viking(level, LV_OBJ_ERIK, 88, 128, vikings_flags);
        add_viking(level, LV_OBJ_BALEOG, 200, 128,
                   vikings_flags | LV_OBJ_FLAG_FLIP_HORIZ);
        add_viking(level, LV_OBJ_OLAF, 128, 112, vikings_flags);
        break;

    case 0x10:
        /* Fallthrough */
        // FIXME - pushes 0xfff8,0xfff0 as adjustments for xoff
    default:
        add_viking(level, LV_OBJ_ERIK, vikings_xoff, vikings_yoff,
                   vikings_flags);
        if (vikings_flags & LV_OBJ_FLAG_FLIP_HORIZ) {
            add_viking(level, LV_OBJ_BALEOG, vikings_xoff + 0x20, vikings_yoff,
                       vikings_flags);
            add_viking(level, LV_OBJ_OLAF, vikings_xoff + 0x40, vikings_yoff,
                       vikings_flags);
        } else {
            add_viking(level, LV_OBJ_BALEOG, vikings_xoff - 0x20, vikings_yoff,
                       vikings_flags);
            add_viking(level, LV_OBJ_OLAF, vikings_xoff - 0x40, vikings_yoff,
                       vikings_flags);
        }
        break;
    }

    return 0;
}

static int load_objects(struct lv_level *level, struct buffer *buf)
{
    uint16_t xoff, yoff, half_width, half_height, type, flags, arg;

    lv_debug(LV_DEBUG_LEVEL, "Loading objects:");
    while (1) {
        buffer_get_le16(buf, &xoff);
        if (xoff == 0xffff)
            break;
        buffer_get_le16(buf, &yoff);
        buffer_get_le16(buf, &half_width);
        buffer_get_le16(buf, &half_height);
        buffer_get_le16(buf, &type);
        buffer_get_le16(buf, &flags);
        buffer_get_le16(buf, &arg);

        add_object(level, type, xoff, yoff, half_width * 2, half_height * 2,
                   flags, arg);

        lv_debug(LV_DEBUG_LEVEL, "  [%.2zx] type=%.4x, pos=(%4d,%4d), size=(%4d,%4d), flags=%.4x, arg=%.4x",
                 level->num_objects, type, xoff, yoff, half_width * 2,
                 half_height * 2, flags, arg);
    }

    return 0;
}

static int load_palette(struct lv_pack *pack, struct lv_level *level,
                        struct buffer *buf)
{
    struct lv_chunk *chunk;
    unsigned base, size;
    uint16_t chunk_index;
    uint8_t base_color;
    uint8_t *data;

    /*
     * Entries are 3-bytes
     *   [00] u16: Palette chunk index - 0xffff ends
     *   [02]  u8: Base color
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading palettes:");
    while (1) {
        buffer_get_le16(buf, &chunk_index);
        if (chunk_index == 0xffff)
            break;
        buffer_get_u8(buf, &base_color);

        chunk = lv_pack_get_chunk(pack, chunk_index);
        if (!chunk)
            continue;

        lv_decompress_chunk(chunk, &data);

        lv_debug(LV_DEBUG_LEVEL, "  Chunk %.4x, base_color=%.3d (%.2zd colors)",
                 chunk_index, base_color, chunk->decompressed_size / 3);

        chunk = lv_pack_get_chunk(pack, chunk_index);
        lv_decompress_chunk(chunk, &data);

        base = base_color * 3;
        size = chunk->decompressed_size;
        if (base + size > sizeof(level->palette))
            size = sizeof(level->palette) - base;

        memcpy(&level->palette[base], data, size);
        free(data);
    }

    return 0;
}

static int load_palette_animations(struct lv_pack *pack, struct lv_level *level,
                                   struct buffer *buf)
{
    uint8_t a, b, c;
    uint16_t val;

    /*
     * Header: u16 - unknown
     *
     * Entries are 3 byte header, plus n u16 values:
     *   [00]: u8 - 0x00 ends
     *   [01]: u8
     *   [02]: u8
     *   [03]: u16 * n - 0xffff ends
     *
     * Maximum 8 entries
     */
    buffer_get_le16(buf, &val);
    lv_debug(LV_DEBUG_LEVEL, "Loading palette animations: header=%.4x", val);
    while (1) {
        buffer_get_u8(buf, &a);
        if (a == 0x00)
            break;

        buffer_get_u8(buf, &b);
        buffer_get_u8(buf, &c);

        lv_debug(LV_DEBUG_LEVEL, "  %.2x [%.2x:%.2x]", a, b, c);

        while (1) {
            buffer_get_le16(buf, &val);
            if (val == 0xffff)
                break;

            lv_debug(LV_DEBUG_LEVEL, "    %.4x", val);
        }
    }

    return 0;
}

static int load_unpacked_sprite_sets(struct lv_pack *pack,
                                     struct lv_level *level, struct buffer *buf)
{
    struct lv_sprite_set *set;
    struct lv_chunk *chunk;
    uint16_t chunk_index, a, b;

    /*
     * Entries are 6 bytes:
     *   [00] u16: Chunk index - 0xffff ends
     *   [02] u16:
     *   [04] u16:
     *
     * The maximum combined data size of all chunks is 0x7000 bytes.
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading unpacked sprite sets:");
    while (1) {
        buffer_get_le16(buf, &chunk_index);
        if (chunk_index == 0xffff)
            break;
        buffer_get_le16(buf, &a);
        buffer_get_le16(buf, &b);

        /* Load the set */
        set = &level->sprite_unpacked_sets[level->num_sprite_unpacked_sets];
        chunk = lv_pack_get_chunk(pack, chunk_index);
        lv_decompress_chunk(chunk, &set->planar_data);

        /*
         * Number of sprites is initialised later when the sprite
         * size is known.
         */
        set->chunk_index = chunk_index;
        set->data_size = chunk->decompressed_size;
        set->num_sprites = 0;

        lv_debug(LV_DEBUG_LEVEL, "  [%.2zx] chunk %.4d (%.4x): %.4x:%.4x",
		 level->num_sprite_unpacked_sets, set->chunk_index,
		 set->chunk_index, a, b);

        level->num_sprite_unpacked_sets++;
    }

    return 0;
}

static int load_sprite32_sets(struct lv_pack *pack, struct lv_level *level,
                              struct buffer *buf)
{
    struct lv_sprite_set *set;
    struct lv_chunk *chunk;
    uint16_t chunk_index, offset, first_offset;
    struct buffer sprites_buf;
    uint8_t a, b, c;

    /*
     * Entries are 5 bytes:
     *   [00] u16: Chunk index - 0xffff ends
     *   [02]  u8: Unknown
     *   [03]  u8: Unknown
     *   [04]  u8: Unknown
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading 32x32 sprite sets:");
    while (1) {
        buffer_get_le16(buf, &chunk_index);
        if (chunk_index == 0xffff)
            break;
        buffer_get_u8(buf, &a);
        buffer_get_u8(buf, &b);
        buffer_get_u8(buf, &c);

        /* Load the set */
        set = &level->sprite32_sets[level->num_sprite32_sets];
        chunk = lv_pack_get_chunk(pack, chunk_index);
        lv_decompress_chunk(chunk, &set->planar_data);

        set->data_size = chunk->decompressed_size;
        buffer_init_from_data(&sprites_buf, set->planar_data, set->data_size);
        set->num_sprites = 0;
        buffer_peek_le16(&sprites_buf, 0, &first_offset);
        while (1) {
            buffer_get_le16(&sprites_buf, &offset);
            if (buffer_offset(&sprites_buf) >= first_offset)
                break;

            set->sprites[set->num_sprites++] = set->planar_data + offset;
            set->format = LV_SPRITE_FORMAT_PACKED;
        }

        lv_debug(LV_DEBUG_LEVEL,
                 "  [%.2zx] Chunk=%.4d (%.4x), num_sprites=%2zd, %.2x:%.2x:%.2x",
                 level->num_sprite32_sets, chunk_index, chunk_index,
                 set->num_sprites, a, b, c);

        level->num_sprite32_sets++;
    }

    return 0;
}

static void update_unpacked_sprite_sets(struct lv_level *level)
{
    struct lv_object *obj;
    struct lv_sprite_set *set;
    size_t tile_size, sprite_size;
    int i, j;

    lv_debug(LV_DEBUG_LEVEL, "Updating unpacked sprite sets:");
    for (i = 0; i < level->num_objects; i++) {
        obj = &level->objects[i];

        lv_object_db_get_object(&level->object_db, obj->type, &obj->db_entry);
        if (obj->db_entry.chunk_sprites == LV_OBJECT_DB_SPRITES_NONE ||
            obj->db_entry.chunk_sprites == LV_OBJECT_DB_SPRITES_PACKED)
            continue;

        /* Object has unpacked sprites. Find the corresponding set */
        for (j = 0; j < level->num_sprite_unpacked_sets; j++) {
            set = &level->sprite_unpacked_sets[j];

            if (set->chunk_index == obj->db_entry.chunk_sprites) {
                obj->sprite_set = set;
                break;
            }
        }

        if (obj->sprite_set) {
            if (set->num_sprites) {
                /* Already processed this sprite set */
                continue;
            }

            /*
             * Update the sprite entries for this set. Unpacked sprites
             * have 9 bytes per 8 pixels (mask + pixel data).
             */
            tile_size = min(obj->width, obj->height);
	    sprite_size = lv_sprite_unpacked_data_size(tile_size, tile_size);
            if (sprite_size == 0)
                continue;

            set->format = LV_SPRITE_FORMAT_UNPACKED;
            set->num_sprites = set->data_size / sprite_size;
            for (j = 0; j < set->num_sprites; j++)
                set->sprites[j] = &set->planar_data[j * sprite_size];

            lv_debug(LV_DEBUG_LEVEL,
                     "  Chunk %.4x (%.4d) has %2zd %2dx%2d unpacked sprites",
                     obj->db_entry.chunk_sprites, obj->db_entry.chunk_sprites,
                     set->num_sprites,
                     obj->db_entry.width, obj->db_entry.height);
        }
    }
}

int lv_level_load(struct lv_pack *pack, struct lv_level *level,
                  unsigned chunk_header, unsigned chunk_object_db)
{
    struct buffer buf;
    struct lv_chunk *chunk;
    uint8_t *data;

    memset(level, 0, sizeof(*level));

    chunk = lv_pack_get_chunk(pack, chunk_header);
    lv_decompress_chunk(chunk, &data);

    buffer_init_from_data(&buf, data, chunk->decompressed_size);
    load_header(pack, level, &buf);
    load_objects(level, &buf);
    load_palette(pack, level, &buf);
    load_palette_animations(pack, level, &buf);
    load_unpacked_sprite_sets(pack, level, &buf);
    load_sprite32_sets(pack, level, &buf);
    free(data);

    /* Load the object database */
    if (chunk_object_db != 0xffff) {
        lv_object_db_load(pack, &level->object_db, chunk_object_db);
        update_unpacked_sprite_sets(level);
    }

    return 0;
}
