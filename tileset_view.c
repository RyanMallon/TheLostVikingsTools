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
#include <liblv/common.h>

#define TILE_SIZE      8
#define TILE_DATA_SIZE (TILE_SIZE * TILE_SIZE)

#define PREFAB_SIZE    (TILE_SIZE * 2)

#define GAP            2
#define SCREEN_WIDTH   ((PREFAB_SIZE + GAP) * 32)
#define SCREEN_HEIGHT  ((PREFAB_SIZE + GAP) * 32)


static void draw_tile(SDL_Surface *surf, const uint8_t *sprite_data,
                      unsigned tile, unsigned flags, unsigned x, unsigned y)
{
    bool flip_horiz, flip_vert;

    flip_horiz = !!(flags & LV_PREFAB_FLAG_FLIP_HORIZ);
    flip_vert  = !!(flags & LV_PREFAB_FLAG_FLIP_VERT);

    lv_sprite_draw_raw(&sprite_data[TILE_DATA_SIZE * tile],
                       TILE_SIZE, TILE_SIZE,
                       flip_horiz, flip_vert, surf->pixels, x, y, surf->w);
}

static void draw_prefab(SDL_Surface *surf, const uint8_t *sprite_data,
                        struct lv_tile_prefab *prefab, unsigned x, unsigned y)
{
    draw_tile(surf, sprite_data, prefab->tile[0], prefab->flags[0], x, y);
    draw_tile(surf, sprite_data, prefab->tile[1], prefab->flags[1], x + TILE_SIZE, y);
    draw_tile(surf, sprite_data, prefab->tile[2], prefab->flags[2], x, y + TILE_SIZE);
    draw_tile(surf, sprite_data, prefab->tile[3], prefab->flags[3], x + TILE_SIZE, y + TILE_SIZE);
}

static void load_palette_from_chunk(struct lv_pack *pack, SDL_Surface *surf,
                                    unsigned chunk_index, bool uncompressed)
{
    SDL_Color sdl_pal[256];
    uint8_t *pal_data;
    size_t pal_size;
    struct lv_chunk *chunk;
    int i;

    chunk = lv_pack_get_chunk(pack, chunk_index);
    if (uncompressed)
        pal_data = chunk->data + 4;
    else
        lv_decompress_chunk(chunk, &pal_data);
    pal_size = chunk->decompressed_size - 1;

    for (i = 0; i < pal_size / 3; i++) {
        sdl_pal[i].r = pal_data[(i * 3) + 0] << 2;
        sdl_pal[i].g = pal_data[(i * 3) + 1] << 2;
        sdl_pal[i].b = pal_data[(i * 3) + 2] << 2;
    }

    SDL_SetPalette(surf, SDL_LOGPAL | SDL_PHYSPAL, sdl_pal, 0, pal_size / 3);
}

static void usage(const char *progname, int status)
{
    printf("Usage: %s [OPTIONS...] PACK_FILE TILESET_CHUNK PREFABS_CHUNK\n",
           progname);
    printf("\nOptions:\n");
    printf("  -B, --blackthorne  Pack file is Blackthorne format\n");
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
        {"palette",       required_argument, 0, 'p'},
    };
    const char *short_options = "Bp:";
    SDL_Surface *screen;
    struct lv_tile_prefab *prefabs;
    size_t num_prefabs, screen_width = SCREEN_WIDTH,
        screen_height = SCREEN_HEIGHT;
    bool blackthorne = false;
    char *pack_filename;
    uint8_t *sprite_data;
    struct lv_pack pack;
    struct lv_chunk *chunk;
    unsigned tileset_chunk_index, prefabs_chunk_index, i, x = 0, y = 0;
    int c, option_index, pal_chunk_index = -1;

    while (1) {
        c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'B':
            blackthorne = true;
            break;

        case 'p':
            pal_chunk_index = strtoul(optarg, NULL, 0);
            break;

	default:
            printf("Unknown argument '%c'\n", c);
            usage(argv[0], EXIT_FAILURE);
	}
    }

    if (optind == argc - 3) {
        pack_filename = argv[optind++];
        tileset_chunk_index = strtoul(argv[optind++], NULL, 0);
        prefabs_chunk_index = strtoul(argv[optind++], NULL, 0);
    } else {
        printf("No data file or chunks specified\n");
        usage(argv[0], EXIT_FAILURE);
    }

    lv_pack_load(pack_filename, &pack, blackthorne);

    /* Load the tileset sprite data */
    chunk = lv_pack_get_chunk(&pack, tileset_chunk_index);
    lv_decompress_chunk(chunk, &sprite_data);

    /* Load the prefabs */
    lv_load_tile_prefabs(&pack, &prefabs, &num_prefabs, prefabs_chunk_index);
    printf("Loaded %zd prefabs\n", num_prefabs);

    screen = SDL_SetVideoMode(screen_width, screen_height, 8, SDL_INIT_VIDEO);

    if (pal_chunk_index >= 0)
        load_palette_from_chunk(&pack, screen, pal_chunk_index, false);

    for (i = 0; i < num_prefabs; i++) {
        draw_prefab(screen, sprite_data, &prefabs[i], x, y);

        x += PREFAB_SIZE + GAP;
        if (x > SCREEN_WIDTH - (PREFAB_SIZE + GAP)) {
            x = 0;
            y += PREFAB_SIZE + GAP;
        }
    }

    SDL_Flip(screen);
    getchar();

    exit(EXIT_SUCCESS);

}
