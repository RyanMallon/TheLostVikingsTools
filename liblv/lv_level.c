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
static const struct lv_level_info lv_level_info[] = {
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

/* These are hardcoded in BTHORNE.EXE */
static const struct lv_level_info bt_level_info[] = {
    /* Cutscene - game start */
    {0x7b, 0xffff},

    /* Mines */
    {0xc0, 0xffff}, {0xc1, 0xffff}, {0xc2, 0xffff}, {0xc3, 0xffff},

    /* Forest */
    {0xcc, 0xffff}, {0xcd, 0xffff}, {0xce, 0xffff}, {0xcf, 0xffff},

    /* Canyons */
    {0xda, 0xffff}, {0xdb, 0xffff}, {0xdc, 0xffff}, {0xdd, 0xffff},

    /* Castle */
    {0xec, 0xffff}, {0xed, 0xffff}, {0xee, 0xffff}, {0xef, 0xffff},

    /* Sarlac boss fight level */
    {0x0f0, 0xffff},

    /* Cutscenes */
    {0x0c5, 0xffff}, {0x0d0, 0xffff}, {0x0de, 0xffff}, {0x15b, 0xffff},

    /* Game over */
    {0x075, 0xffff},
};

const struct lv_level_info *lv_level_get_info(struct lv_pack *pack,
                                              unsigned level_num)
{
    const struct lv_level_info *level_info;
    size_t num_levels;

    if (pack->blackthorne) {
        level_info = bt_level_info;
        num_levels = ARRAY_SIZE(bt_level_info);
    } else {
        level_info = lv_level_info;
        num_levels = ARRAY_SIZE(lv_level_info);
    }

    if (level_num < 1 || level_num >= num_levels)
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
                    unsigned chunk_index, uint16_t width, uint16_t height,
                    uint16_t **map)
{
    struct lv_chunk *chunk;
    uint8_t *data;
    size_t map_size;

    map_size = width * height * sizeof(uint16_t);

    chunk = lv_pack_get_chunk(pack, chunk_index);
    if (chunk->decompressed_size != map_size)
        return -1;

    lv_decompress_chunk(chunk, &data);
    *map = (uint16_t *)data;

    return 0;
}

int lv_load_tile_prefabs(struct lv_pack *pack,
			 struct lv_tile_prefab **r_prefabs,
			 size_t *r_num_prefabs, unsigned chunk_index)
{
    struct lv_chunk *chunk;
    struct lv_tile_prefab *prefabs;
    size_t num_prefabs;
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
    num_prefabs = chunk->decompressed_size / 8;
    prefabs = calloc(num_prefabs, sizeof(*prefabs));

    for (i = 0; i < num_prefabs; i++) {
        for (j = 0; j < 4; j++) {
            buffer_get_le16(&buf, &val);

            /* Bits 7..6 the base for the tile index */
            base = (val >> 6) & 0x3;

            /* Upper byte is the tile index */
            prefabs[i].tile[j] = base + ((val >> 8) * 4);

            /* Lower 6-bits are flags */
            prefabs[i].flags[j] = val & 0x3f;
        }
    }

    free(data);

    *r_prefabs = prefabs;
    *r_num_prefabs = num_prefabs;

    return 0;
}

static int load_lv_header(struct lv_pack *pack, struct lv_level *level,
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
     *
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

    load_map(pack, level, chunk_map, level->width, level->height, &level->map);
    lv_load_tile_prefabs(pack, &level->prefabs, &level->num_prefabs,
			 chunk_prefabs);

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

    /*
     * Object entries (14 bytes):
     *   [00] u16: xoffset - 0xffff ends
     *   [02] u16: yoffset
     *   [04] u16: width / 2
     *   [06] u16: height / 2
     *   [08] u16: type (in object database)
     *   [0a] u16: flags
     *   [0c] u16: argument
     */
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
     * Entries are 3-bytes (Blackthorne limits to 8 entries)
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

        lv_debug(LV_DEBUG_LEVEL, "  Chunk %.4x, base_color=%02x (%3zd colors)",
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
    struct lv_pal_animation *anim;
    uint8_t counter, index1, index2;
    uint16_t flags, val;
    int i, j;

    /*
     * Header:
     *   [00]     u16: Flags
     *
     * Entries are 3 byte header, plus n u16 values:
     *   [00]:     u8: Count down start - 0x00 ends
     *   [01]:     u8: First color index
     *   [02]:     u8: Second color index
     *   [03]: u16[n]: Animation values - 0xffff ends
     *
     * Maximum 8 entries
     */
    buffer_get_le16(buf, &flags);
    lv_debug(LV_DEBUG_LEVEL, "Loading palette animations: flags=%.4x", flags);

    level->pal_animation_flags = flags;
    level->num_pal_animations  = 0;

    for (i = 0; i < ARRAY_SIZE(level->pal_animation); i++) {
        anim = &level->pal_animation[i];

        buffer_get_u8(buf, &counter);
        if (counter == 0x00)
            break;

        buffer_get_u8(buf, &index1);
        buffer_get_u8(buf, &index2);

        anim->max_counter = counter;
        anim->index1 = index1;
        anim->index2 = index2;

        lv_debug(LV_DEBUG_LEVEL, "  counter=%.2x, index=%.2x:%.2x",
                 counter, index1, index2);

        // FIXME - maximum?
        anim->num_values = 0;
        for (j = 0; ; j++) {
            buffer_get_le16(buf, &val);
            if (val == 0xffff)
                break;

            anim->values[j] = val;
            anim->num_values++;

            lv_debug(LV_DEBUG_LEVEL, "    %.4x", val);
        }

        level->num_pal_animations++;
    }

    return 0;
}

static int load_raw_sprite_sets(struct lv_pack *pack, struct lv_level *level,
                                struct buffer *buf)
{
    uint16_t chunk_index;
    uint8_t a, b, c;

    /*
     * Entries are 5 bytes (max 16 entries)
     * u16: 0xffff ends
     *  u8:
     *  u8: Multiplied with previous
     *  u8:
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading raw sprite sets:");
    while (1) {
        buffer_get_le16(buf, &chunk_index);
        if (chunk_index == 0xffff)
            break;
        buffer_get_u8(buf, &a);
        buffer_get_u8(buf, &b);
        buffer_get_u8(buf, &c);

        lv_debug(LV_DEBUG_LEVEL, "  Chunk=%03x: %.2x %.2x %.2x",
                 chunk_index, a, b, c);
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
     * Entries are 6 bytes (Blackthorne limits to 32 entries)
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

        lv_debug(LV_DEBUG_LEVEL, "  [%.2zx] chunk %03x: %.4x:%.4x",
		 level->num_sprite_unpacked_sets, set->chunk_index, a, b);

        level->num_sprite_unpacked_sets++;
    }

    return 0;
}

static int load_sprite32_sets(struct lv_pack *pack, struct lv_level *level,
                              struct buffer *buf)
{
    struct lv_sprite_set *set;
    struct lv_chunk *chunk;
    uint16_t chunk_index;
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
        lv_sprite_load_set(set, LV_SPRITE_FORMAT_PACKED32, 32, 32, chunk);

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
	    sprite_size = lv_sprite_data_size(LV_SPRITE_FORMAT_UNPACKED,
                                              tile_size, tile_size);
            if (sprite_size == 0)
                continue;

            set->format = LV_SPRITE_FORMAT_UNPACKED;
            set->num_sprites = set->data_size / sprite_size;
            set->sprites = calloc(set->data_size, sizeof(uint8_t *));

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

static void load_something(struct lv_pack *pack, struct lv_level *level,
                           struct buffer *buf)
{
    uint16_t header, e, f;
    uint8_t a, b, c, d;

    /*
     * Header: u16 - unknown
     *
     * Entries are 8 bytes:
     *   [00]  u8: Unknown - zero ends
     *   [01]  u8:
     *   [02]  u8:
     *   [03]  u8:
     *   [04] u16:
     *   [06] u16: chunk index?
     */
    buffer_get_le16(buf, &header);
    lv_debug(LV_DEBUG_LEVEL, "Loading something: header=%.4x", header);
    while (1) {
        buffer_get_u8(buf, &a);
        if (a == 0)
            break;
        buffer_get_u8(buf, &b);
        buffer_get_u8(buf, &c);
        buffer_get_u8(buf, &d);
        buffer_get_le16(buf, &e);
        buffer_get_le16(buf, &f);
        lv_debug(LV_DEBUG_LEVEL, "  %.2x %.2x %.2x %.2x %.4x %.4x",
                 a, b, c, d, e, f);
    }
}

static void load_level_exit(struct lv_pack *pack, struct lv_level *level,
                            struct buffer *buf)
{
    uint8_t x1, y1, x2, y2;
    uint16_t field0, field1, field2;

    /*
     * Entries are 10 bytes:
     *   [00]   u8: x offset? - 0xff ends
     *   [01]   u8: y offset?
     *   [02]   u8: x offset?
     *   [03]   u8: y offset?
     *   [04]  u16: next level header chunk index
     *   [06]  u16: level info field 1
     *   [08]  u16: level info field 2
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading level exit info:");
    while (1) {
        buffer_get_u8(buf, &x1);
        if (x1 == 0xff)
            break;
        buffer_get_u8(buf, &y1);
        buffer_get_u8(buf, &x2);
        buffer_get_u8(buf, &y2);
        buffer_get_le16(buf, &field0);
        buffer_get_le16(buf, &field1);
        buffer_get_le16(buf, &field2);

        lv_debug(LV_DEBUG_LEVEL,
                 "  (%.2x, %.2x)-(%.2x, %.2x) %.4x %.4x %.4x",
                 x1, y1, x2, y2, field0, field1, field2);
    }
}

static void load_something3(struct lv_pack *pack, struct lv_level *level,
                            struct buffer *buf)
{
    uint16_t a, size, offset = 0;
    uint8_t b, c;

    /*
     * Entries are 4 bytes:
     *   [00] u16: 0xffff ends
     *   [02]  u8:
     *   [03]  u8:
     */
    lv_debug(LV_DEBUG_LEVEL, "Loading something3");
    while (1) {
        buffer_get_le16(buf, &a);
        if (a == 0xffff)
            break;
        buffer_get_u8(buf, &b);
        buffer_get_u8(buf, &c);

        /*
         * The two operands are used to calculate an offset for the next
         * entry.
         */
        size = (b * c) * 0x48;

        lv_debug(LV_DEBUG_LEVEL, "  %.4x: %.2x %.2x (%dx%d): size=%.4x, offset=%.4x",
                 a, b, c, b << 3, c << 3, size, offset);
        offset += size;
    }

}

static int load_bt_level(struct lv_pack *pack, struct lv_level *level,
                         struct buffer *buf)
{
    uint16_t width, height, chunk_index_map, chunk_index_tileset,
        chunk_index_prefabs, bg_width, bg_height, chunk_index_bg_map,
        chunk_index_bg_tileset, chunk_index_bg_prefabs;
    uint8_t dummy;

    /*
     * [08]  u8: flags: bit1=load hud items
     *
     * [1c] u16: Level width (tiles)
     * [1e] u16: Level height (tiles)
     * [20]  u8: Unknown
     * [21] u16: Level map chunk index
     * [23] u16: Level tileset chunk index
     * [25] u16: Level prefabs chunk index
     * [27] u16: Background width (tiles)
     * [29] u16: Background height (tiles)
     * [2b]  u8: Unknown
     * [2c] u16: Background map chunk index
     * [2e] u16: Background tileset chunk index?
     * [30] u16: Background prefabs chunk index?
     */
    buffer_seek(buf, 0x1c);
    buffer_get_le16(buf, &width);
    buffer_get_le16(buf, &height);
    buffer_get_u8(buf, &dummy);
    buffer_get_le16(buf, &chunk_index_map);
    buffer_get_le16(buf, &chunk_index_tileset);
    buffer_get_le16(buf, &chunk_index_prefabs);

    buffer_get_le16(buf, &bg_width);
    buffer_get_le16(buf, &bg_height);
    buffer_get_u8(buf, &dummy);
    buffer_get_le16(buf, &chunk_index_bg_map);
    buffer_get_le16(buf, &chunk_index_bg_tileset);
    buffer_get_le16(buf, &chunk_index_bg_prefabs);

    // FIXME - handle different background size and tileset/prefabs

    lv_debug(LV_DEBUG_LEVEL, "  Map size:         %dx%d (%dx%d rooms)",
             width, height, width / 16, height / 14);
    lv_debug(LV_DEBUG_LEVEL, "  Map chunk:        %3x", chunk_index_map);
    lv_debug(LV_DEBUG_LEVEL, "  Tileset chunk:    %3x", chunk_index_tileset);
    lv_debug(LV_DEBUG_LEVEL, "  Prefabs chunk:    %3x", chunk_index_prefabs);

    lv_debug(LV_DEBUG_LEVEL, "  BG map chunk:     %3x", chunk_index_bg_map);
    lv_debug(LV_DEBUG_LEVEL, "  BG tileset chunk: %3x", chunk_index_bg_tileset);
    lv_debug(LV_DEBUG_LEVEL, "  BG prefabs chunk: %3x", chunk_index_bg_prefabs);

    level->width = width;
    level->height = height;
    level->chunk_tileset = chunk_index_tileset;

    /* Load the main and optional background maps */
    load_map(pack, level, chunk_index_map,
             level->width, level->height, &level->map);
    if (chunk_index_bg_map != 0xffff)
        load_map(pack, level, chunk_index_bg_map,
                 level->width, level->height, &level->bg_map);

    lv_load_tile_prefabs(pack, &level->prefabs, &level->num_prefabs,
                         chunk_index_prefabs);

    buffer_seek(buf, 0x36);
    load_objects(level, buf);
    load_palette(pack, level, buf);
    load_palette_animations(pack, level, buf);
    load_something(pack, level, buf);
    load_unpacked_sprite_sets(pack, level, buf);
    load_raw_sprite_sets(pack, level, buf);
    load_level_exit(pack, level, buf);
    load_something3(pack, level, buf);

    return 0;
}

static int load_lv_level(struct lv_pack *pack, struct lv_level *level,
                         struct buffer *buf, unsigned chunk_object_db)
{
    load_lv_header(pack, level, buf);
    load_objects(level, buf);
    load_palette(pack, level, buf);
    load_palette_animations(pack, level, buf);
    load_unpacked_sprite_sets(pack, level, buf);
    load_sprite32_sets(pack, level, buf);

    /* Load the object database */
    if (chunk_object_db != 0xffff) {
        lv_object_db_load(pack, &level->object_db, chunk_object_db);
        update_unpacked_sprite_sets(level);
    }

    return 0;
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

    if (pack->blackthorne)
        load_bt_level(pack, level, &buf);
    else
        load_lv_level(pack, level, &buf, chunk_object_db);

    free(data);
    return 0;
}
