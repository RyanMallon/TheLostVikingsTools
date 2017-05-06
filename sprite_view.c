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

#define SCREEN_WIDTH	(64 * 16)
#define SCREEN_HEIGHT	(48 * 16)

static const char *format_names[] = {
    [LV_SPRITE_FORMAT_RAW]      = "raw",
    [LV_SPRITE_FORMAT_UNPACKED] = "unpacked",
    [LV_SPRITE_FORMAT_PACKED32] = "packed32",
};

static void load_palette_from_level(struct lv_pack *pack, SDL_Surface *surf,
                                    unsigned level_num)
{
    const struct lv_level_info *level_info;
    struct lv_level level;
    SDL_Color sdl_pal[256];
    int i;

    /* Load level to get full palette */
    level_info = lv_level_get_info(pack, level_num);
    if (!level_info) {
        printf("Bad level number\n");
        exit(EXIT_FAILURE);
    }

    lv_level_load(pack, &level, level_info->chunk_level_header, 0xffff);

    for (i = 0; i < 256; i++) {
        sdl_pal[i].r = level.palette[(i * 3) + 0] << 2;
        sdl_pal[i].g = level.palette[(i * 3) + 1] << 2;
        sdl_pal[i].b = level.palette[(i * 3) + 2] << 2;
    }

    SDL_SetPalette(surf, SDL_LOGPAL | SDL_PHYSPAL, sdl_pal, 0, 256);
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
    int i;

    printf("Usage: %s [OPTIONS...] FILE SPRITE_CHUNK\n", progname);
    printf("\nOptions:\n");
    printf("  -B, --blackthorne           Pack file is Blackthorne format\n");
    printf("  -f, --format=FORMAT         Sprite format\n");
    printf("  -p, --palette-chunk=INDEX   Use palette from chunk\n");
    printf("  -l, --level=LEVEL           Use palette data from level\n");
    printf("  -b, --palette-base=BASE     Base palette offset for packed32 sprites\n" );
    printf("  -u, --uncompressed          Chunk is uncompressed\n");
    printf("  -s, --splash                Chunk is a splash screen image\n");
    printf("  -w, --width=WIDTH           Sprite width\n");
    printf("  -h, --height=HEIGHT         Sprite height\n");

    printf("  -W, --screen-width=WIDTH    Screen width (default=%d)\n", SCREEN_WIDTH);
    printf("  -H, --screen-height=HEIGHT  Screen height (default=%d)\n", SCREEN_HEIGHT);

    printf("\nFormats:\n");
    for (i = 0; i < ARRAY_SIZE(format_names); i++)
        printf("    %s\n", format_names[i]);
    exit(status);
}

/*
 * Lost Vikings Examples
 * ---------------------
 *
 * View tileset for level 1:
 *   ./sprite_view DATA.DAT 195 -l1 -fraw -w8 -h8
 *
 * View Erik sprites:
 *   ./sprite_view DATA.DAT 224 -l1 -fpacked32 -b0xb0
 *
 * View level 1 gun turret sprites:
 *   ./sprite_view DATA.DAT 233 -l1 -funpacked -w32 -h32
 *
 * View font set/speech bubble sprites:
 *
 *   ./sprite_view DATA.DAT 2 -l1 -funpacked -w8 -h8
 *
 * View Erik HUD image:
 *   ./sprite_view DATA.DAT 4 -l1 -fraw -w32 -h24
 *
 * Blackthorne Examples
 * --------------------
 *
 * View player sprites:
 *
 *   ./sprite_view --blackthorne DATA.DAT -fraw -w32 -h48 -l2 0x42 -b0x80
 *
 */
int main(int argc, char **argv)
{
    const struct option long_options[] = {
        {"blackthorne",   no_argument,       0, 'B'},
        {"format",        required_argument, 0, 'f'},
        {"level",         required_argument, 0, 'l'},
        {"palette-chunk", required_argument, 0, 'p'},
        {"palette-base",  required_argument, 0, 'b'},
        {"uncompressed",  no_argument,       0, 'u'},
        {"splash",        no_argument,       0, 's'},
        {"width",         required_argument, 0, 'w'},
        {"height",        required_argument, 0, 'h'},
        {"screen-width",  required_argument, 0, 'W'},
        {"screen-height", required_argument, 0, 'H'},
        {"help",          no_argument,       0, '?'},
        {NULL, 0, 0, 0},
    };
    const char *short_options = "Bf:l:p:b:usw:h:?";
    const char *pack_filename = NULL;
    SDL_Surface *screen;
    uint8_t *sprite_data;
    struct lv_pack pack;
    struct lv_chunk *chunk;
    struct lv_sprite_set sprite_set;
    bool blackthorne = false, uncompressed = false, splash = false;
    size_t sprite_width = 32, sprite_height = 32,
        screen_width = SCREEN_WIDTH, screen_height = SCREEN_HEIGHT, data_size;
    unsigned format = LV_SPRITE_FORMAT_RAW, chunk_index, level_num = 0, x, y;
    int c, i, option_index, pal_base = 0, pal_chunk_index = -1;

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

        case 'b':
            pal_base = strtoul(optarg, NULL, 0);
            if (pal_base < 0 || pal_base > 255) {
                printf("Invalid palette base\n");
                usage(argv[0], EXIT_FAILURE);
            }
            break;

	case 'l':
            level_num = strtoul(optarg, NULL, 0);
            break;

        case 'w':
            sprite_width = strtoul(optarg, NULL, 0);
            break;

        case 'h':
            sprite_height = strtoul(optarg, NULL, 0);
            break;

        case 'f':
            for (i = 0; i < ARRAY_SIZE(format_names); i++) {
                if (strcmp(optarg, format_names[i]) == 0) {
                    format = i;
                    break;
                }
            }
            if (i == ARRAY_SIZE(format_names)) {
                printf("Bad format '%s'\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;

        case 'u':
            uncompressed = true;
            break;

        case 's':
            splash = true;
            break;

        case 'W':
            screen_width = strtoul(optarg, NULL, 0);
            break;

        case 'H':
            screen_height = strtoul(optarg, NULL, 0);
            break;

        case '?':
            usage(argv[0], EXIT_SUCCESS);
            break;

        default:
            printf("Unknown argument '%c'\n", c);
            usage(argv[0], EXIT_FAILURE);
        }
    }

    if (optind == argc - 2) {
        pack_filename = argv[optind++];
        chunk_index = strtoul(argv[optind++], NULL, 0);
    } else {
        printf("No data file or chunk specified\n");
        usage(argv[0], EXIT_FAILURE);
    }

    lv_pack_load(pack_filename, &pack, blackthorne);
    chunk = lv_pack_get_chunk(&pack, chunk_index);
    if (uncompressed)
        sprite_data = chunk->data + chunk->data_offset;
    else
        lv_decompress_chunk(chunk, &sprite_data);

    screen = SDL_SetVideoMode(screen_width, screen_height, 8, SDL_INIT_VIDEO);

    if (level_num > 0)
        load_palette_from_level(&pack, screen, level_num);
    if (pal_chunk_index > 0)
        load_palette_from_chunk(&pack, screen, pal_chunk_index, false);

    data_size = chunk->decompressed_size;
    if (splash) {
        /*
         * Splash screen chunks store the data size as the number of bytes
         * per plane, so it needs to be multiplied by 4.
         */
        data_size *= 4;
    }

    lv_sprite_load_set(&sprite_set, format, sprite_width, sprite_height, chunk);
    printf("%zd sprites\n", sprite_set.num_sprites);
    for (i = 0, x = 0, y = 0; i < sprite_set.num_sprites; i++) {
        lv_sprite_draw(sprite_set.sprites[i],
                       sprite_width, sprite_height, format,
                       pal_base, false, false, screen->pixels, x, y, screen->w);

        x += sprite_width;
        if (x > screen->w - sprite_width) {
            x = 0;
            y += sprite_height;
            if (y >= screen->h)
                break;
        }
    }

    SDL_Flip(screen);
    getchar();

    exit(EXIT_SUCCESS);
}
