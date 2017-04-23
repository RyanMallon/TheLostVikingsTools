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

#ifndef _LV_LEVEL_H
#define _LV_LEVEL_H

#include <stdint.h>

#include "lv_sprite.h"
#include "lv_object_db.h"
#include "common.h"

/**
 * \defgroup lv_level Level
 * \{
 *
 * Levels in The Lost Vikings are tile based. The tile sprites are 8x8 images
 * which are combined into 16x16 'prefabs' which are used for the level's
 * tile map. Each level has its own palette set, which may include palette
 * swap animations.
 */

#define LV_PREFAB_INDEX_MASK        0x1ff
#define LV_PREFAB_FLAGS_SHIFT       9
#define LV_PREFAB_FLAGS_MASK        0x7f

#define LV_PREFAB_FLAG_FOREGROUND   0x08
#define LV_PREFAB_FLAG_FLIP_HORIZ   0x10
#define LV_PREFAB_FLAG_FLIP_VERT    0x20
#define LV_PREFAB_FLAG_COLOR_MASK   0x7

/* Maximum array sizes as defined by VIKINGS.EXE */
#define LV_MAX_SPRITE32_SETS   0x10
#define LV_MAX_SPRITE16_SETS   0x20

/* Viking object numbers */
#define LV_OBJ_BALEOG          0
#define LV_OBJ_ERIK            1
#define LV_OBJ_OLAF            2

/* Object flags */
#define LV_OBJ_FLAG_FLIP_HORIZ   0x0040
#define LV_OBJ_FLAG_NO_DRAW      0x0800

/* Sprite formats */
#define LV_SPRITE_FORMAT_PACKED   1
#define LV_SPRITE_FORMAT_UNPACKED 2

struct lv_pack;

/**
 * Information about the chunks used for a given level. These are hardcoded
 * in VIKINGS.EXE. See \ref lv_level_get_info.
 */
struct lv_level_info {
    /** The index of the level header chunk. */
    unsigned  chunk_level_header;

    /** The index of the object database chunk. */
    unsigned  chunk_object_db;
};

/**
 * A tile prefab. Prefab tiles are 16x16 and are constructed from four 8x8
 * tiles from the tileset. The prefab tiles are stored left to right, top to
 * bottom. Flags for each component tile allow the tileset tiles to be
 * flipped vertically and/or horizontally to generate the prefab image.
 * Each component tile may also be foreground or background.
 */
struct lv_tile_prefab {
    /** Tileset images which make up the prefab. */
    uint16_t               tile[4];

    /** Flags for each component tile of the prefab. */
    uint8_t                flags[4];
};

/**
 * A set of sprites. Sprites may be stored in a number of different formats
 * (see \ref lv_sprite). The size of the sprites is managed by the object
 * referring to the sprite set.
 */
struct lv_sprite_set {
    /** The chunk this set of sprites is from. */
    unsigned               chunk_index;

    /** Original planar data from the data file chunk. */
    uint8_t                *planar_data;

    /** Size of the planar data. */
    size_t                 data_size;

    /** Sprite format. */
    unsigned               format;

    /** Pointers to the start of each sprite in the set. */
    uint8_t                *sprites[128];

    /** Number of sprites in the set. */
    size_t                 num_sprites;
};

/** An object in the level. */
struct lv_object {
    /**
     * Object type. This is an index into the object database
     * (see \ref lv_object_db). The object database entry acts as a template
     * class for the object, while this structure is an instance of the
     * object class in the level.
     */
    unsigned               type;

    /** Object center x offset. */
    unsigned               xoff;

    /** Object center y offset. */
    unsigned               yoff;

    /** Object width. */
    unsigned               width;

    /** Object height. */
    unsigned               height;

    /** Object flags. */
    unsigned               flags;

    /**
     * Object specific argument. For objects like buttons this appears to be
     * a mask of tag values that correspond to other objects that this object
     * interacts with. For collectible items it is the type of the item.
     */
    unsigned               arg;

    struct lv_sprite_set   *sprite_set;
    struct lv_object_db_entry db_entry;
};

/**
 * A single level. Special levels are used for the company logos, world warp
 * screen, death screen, etc.
 */
struct lv_level {
    /** Width of the level in 16x16 tiles. */
    unsigned               width;

    /** Height of the level in 16x16 tiles. */
    unsigned               height;

    /** The tileset chunk. */
    unsigned               chunk_tileset;

    /**
     * Object database for this level. The object database chunk is shared
     * across all levels in a world.
     */
    struct lv_object_db    object_db;

    /** Palette. */
    uint8_t                palette[256 * 3];

    /** Tile prefabs. */
    struct lv_tile_prefab  *prefabs;

    /** Number of tile prefabs. */
    size_t                 num_prefabs;

    /** The tile map. */
    uint16_t               *map;

    /** Background tile map. Only used by Blackthorne */
    uint16_t               *bg_map;

    /** Objects in the level. */
    struct lv_object       objects[100];

    /** Number objects in the level. */
    size_t                 num_objects;

    /** Packed 32x32 sprite sets. */
    struct lv_sprite_set   sprite32_sets[LV_MAX_SPRITE32_SETS];

    /** Number of packed 32x32 sprite sets. */
    size_t                 num_sprite32_sets;

    /**
     * Unpacked sprite sets. The size of the sprites is determined by the
     * objects that refer to the sprite set.
     */
    struct lv_sprite_set   sprite_unpacked_sets[LV_MAX_SPRITE16_SETS];

    /** Number of unpacked sprite sets. */
    size_t                 num_sprite_unpacked_sets;
};

/**
 * Get the level information structure for a given level number. This is
 * used to determine which chunks are needed to load the level.
 *
 * \param pack       Pack file.
 * \param level_num  Level number (numbering starts at 1).
 * \returns          Level information or NULL if the level is not found.
 */
const struct lv_level_info *lv_level_get_info(struct lv_pack *pack,
					      unsigned level_num);

/**
 * Load a level. This loads and initialises all data associated with a single
 * level.
 *
 * \param pack            The data pack file.
 * \param level           Level structure to initialise.
 * \param chunk_header    Index of the level header chunk.
 * \param chunk_object_db Index of the level object database chunk.
 * \returns               0 for success.
 */
int lv_level_load(struct lv_pack *pack, struct lv_level *level,
                  unsigned chunk_header, unsigned chunk_object_db);

/**
 * Get the prefab at the given map location in a level.
 *
 * \param level      Level.
 * \param x          Tile x offset.
 * \param y          Tile y offset.
 * \param flags      Optional returned flags.
 * \returns          The prefab at the given location.
 */
static inline struct lv_tile_prefab *
lv_level_get_prefab_at(struct lv_level *level, unsigned x, unsigned y,
                       unsigned *r_tile, unsigned *r_flags)
{
    unsigned index, flags, tile;

    index = level->map[(y * level->width) + x];
    flags = (index >> LV_PREFAB_FLAGS_SHIFT) & LV_PREFAB_FLAGS_MASK;
    tile  = index & 0x3ff;

    if (r_flags)
        *r_flags = flags;
    if (r_tile)
        *r_tile = tile;

    return &level->prefabs[tile];
}

static inline struct lv_tile_prefab *
lv_level_get_bg_prefab_at(struct lv_level *level, unsigned x, unsigned y,
                          unsigned *r_tile, unsigned *r_flags)
{
    unsigned index, flags, tile;

    if (!level->bg_map)
        return NULL;

    index = level->bg_map[(y * level->width) + x];
    flags = (index >> LV_PREFAB_FLAGS_SHIFT) & LV_PREFAB_FLAGS_MASK;
    tile  = index & 0x3ff;

    if (r_flags)
        *r_flags = flags;
    if (r_tile)
        *r_tile = tile;

    return &level->prefabs[tile];
}

/**
 * Load tileset prefab information. Tileset sprites are 8x8 images,
 * but the level tiles are 16x16. The game uses prefabs to stitch
 * together 4 sprites to form a complete tile. The component tiles can
 * be horizontally or vertically flipped when creating a tile.
 *
 * \param pack            Pack file.
 * \param r_prefabs       Returned allocated prefab array.
 * \param r_num_prefabs   Returned number of prefabs.
 * \param chunk_index     Index to load the prefabs from.
 * \returns               0 for success.
 */
int lv_load_tile_prefabs(struct lv_pack *pack,
			 struct lv_tile_prefab **r_prefabs,
			 size_t *r_num_prefabs, unsigned chunk_index);

/** \} */

#endif /* _LV_LEVEL_H */
