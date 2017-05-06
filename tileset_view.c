/*
 * This file is part of The Lost Vikings Library/Tools
 *
 * Ryan Mallon, 2017, <rmallon@gmail.com>
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

#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <SDL/SDL.h>

#include <liblv/lv_pack.h>
#include <liblv/lv_sprite.h>
#include <liblv/lv_level.h>
#include <liblv/lv_debug.h>
#include <liblv/common.h>

#include "sdl_helpers.h"

#define TILE_SIZE      8
#define TILE_DATA_SIZE (TILE_SIZE * TILE_SIZE)

#define PREFAB_SIZE    (TILE_SIZE * 2)

#define GAP            2
#define SCREEN_WIDTH   ((PREFAB_SIZE + GAP) * 32)
#define SCREEN_HEIGHT  ((PREFAB_SIZE + GAP) * 32)

static SDL_Surface *screen;

static struct lv_pack pack;
static struct lv_level level;

static void draw_tile(SDL_Surface *surf, SDL_Surface *surf_tileset,
                      unsigned tile, unsigned flags, unsigned x, unsigned y)
{
    SDL_Rect src_rect, dst_rect;
    uint8_t base_color;

    src_rect.x = tile * TILE_SIZE;
    src_rect.y = 0;
    src_rect.w = TILE_SIZE;
    src_rect.h = TILE_SIZE;

    dst_rect.x = x;
    dst_rect.y = y;
    dst_rect.w = TILE_SIZE;
    dst_rect.h = TILE_SIZE;

    /*
     * The lower bits of the prefab flags specify which set of 16 color
     * palette to use. This is only used by Blackthorne, the bits appear
     * unused by The Lost Vikings.
     */
    base_color = (flags & LV_PREFAB_FLAG_COLOR_MASK) * 0x10;

    sdl_blit(surf_tileset, &src_rect, surf, &dst_rect, base_color,
             flags & LV_PREFAB_FLAG_FLIP_HORIZ,
             flags & LV_PREFAB_FLAG_FLIP_VERT);
}

static void draw_prefab(SDL_Surface *surf, SDL_Surface *surf_tileset,
                        struct lv_tile_prefab *prefab, unsigned x, unsigned y)
{
    draw_tile(surf, surf_tileset, prefab->tile[0], prefab->flags[0], x, y);
    draw_tile(surf, surf_tileset, prefab->tile[1], prefab->flags[1]
              , x + TILE_SIZE, y);
    draw_tile(surf, surf_tileset, prefab->tile[2], prefab->flags[2],
              x, y + TILE_SIZE);
    draw_tile(surf, surf_tileset, prefab->tile[3], prefab->flags[3],
              x + TILE_SIZE, y + TILE_SIZE);
}

static SDL_Surface *load_tileset(unsigned chunk_index)
{
    SDL_Surface *surf;
    struct lv_chunk *chunk;
    size_t num_tiles;
    uint8_t *data, *pixels;
    int i;

    chunk = lv_pack_get_chunk(&pack, chunk_index);
    lv_decompress_chunk(chunk, &data);

    /* Tilesets are 8x8 */
    num_tiles = chunk->decompressed_size / TILE_DATA_SIZE;

    surf = sdl_create_surf(screen, num_tiles * TILE_SIZE, TILE_SIZE);
    if (!surf) {
        printf("Failed to create tile surface: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    /* Render all the tiles in linear vga format on a single surface */
    SDL_LockSurface(surf);
    pixels = surf->pixels;
    for (i = 0; i < num_tiles; i++)
        lv_sprite_draw_raw(data + (i * TILE_DATA_SIZE), 0, TILE_SIZE, TILE_SIZE,
			   false, false, pixels, i * TILE_SIZE, 0, surf->w);
    SDL_UnlockSurface(surf);

    free(data);
    return surf;
}

static void usage(const char *progname, int status)
{
    printf("Usage: %s [OPTIONS...] PACK_FILE LEVEL_NUM\n",
           progname);
    printf("\nOptions:\n");
    printf("  -B, --blackthorne    Pack file is Blackthorne format\n");
    printf("  -d, --debug=FLAGS    Enable debugging\n");
    printf("  -w, --width=WIDTH    Width in tiles\n");
    printf("  -h, --width=HEIGHT   Height in tiles\n");
    printf("  -c, --chunk=CHUNK    Level header chunk (overrides level num)\n");
    exit(status);
}

/*
 * Examples:
 *
 * View the tileset for the spaceship world.
 *   ./tileset_view DATA.DAT 195 197 -p 172
 *
 */
int main(int argc, char **argv)
{
    const struct option long_options[] = {
        {"blackthorne",   no_argument,       0, 'B'},
        {"debug",         required_argument, 0, 'd'},
        {"width",         required_argument, 0, 'w'},
        {"height",        required_argument, 0, 'h'},
        {"chunk",         required_argument, 0, 'c'},
    };
    const char *short_options = "Bd:w:h:c:";
    SDL_Surface *surf_tileset;
    size_t screen_width = SCREEN_WIDTH, screen_height = SCREEN_HEIGHT;
    bool blackthorne = false;
    char *pack_filename;
    const struct lv_level_info *level_info;
    unsigned level_num, i, x = 0, y = 0, debug_flags = 0;
    int c, option_index, chunk_header = -1;

    while (1) {
        c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'B':
            blackthorne = true;
            break;

        case 'd':
            debug_flags = strtoul(optarg, NULL, 0);
            break;

        case 'w':
            screen_width = strtoul(optarg, NULL, 0);
            screen_width *= (PREFAB_SIZE + GAP);
            break;

        case 'h':
            screen_height = strtoul(optarg, NULL, 0);
            screen_height *= (PREFAB_SIZE + GAP);
            break;

        case 'c':
            chunk_header = strtoul(optarg, NULL, 0);
            break;

	default:
            printf("Unknown argument '%c'\n", c);
            usage(argv[0], EXIT_FAILURE);
	}
    }

    if (optind == argc - 2) {
        pack_filename = argv[optind++];
        level_num = strtoul(argv[optind++], NULL, 0);
    } else {
        printf("No data file or level number specified\n");
        usage(argv[0], EXIT_FAILURE);
    }

    if (debug_flags)
        lv_debug_toggle(debug_flags);

    lv_pack_load(pack_filename, &pack, blackthorne);

    level_info = lv_level_get_info(&pack, level_num);
    if (!level_info) {
        printf("Bad level number\n");
        exit(EXIT_FAILURE);
    }

    if (chunk_header == -1)
        chunk_header = level_info->chunk_level_header;

    lv_level_load(&pack, &level, chunk_header, 0xffff);

    /* Load the prefabs */
    printf("Loaded %zd prefabs\n", level.num_prefabs);

    screen = sdl_init(screen_width, screen_height);
    sdl_load_palette(screen, level.palette, 256);
    surf_tileset = load_tileset(level.chunk_tileset);

    for (i = 0; i < level.num_prefabs; i++) {
        draw_prefab(screen, surf_tileset, &level.prefabs[i], x, y);

        x += PREFAB_SIZE + GAP;
        if (x > screen_width - (PREFAB_SIZE + GAP)) {
            x = 0;
            y += PREFAB_SIZE + GAP;
            if (y > screen_height - (PREFAB_SIZE + GAP))
                break;
        }
    }

    SDL_Flip(screen);
    getchar();

    exit(EXIT_SUCCESS);
}
