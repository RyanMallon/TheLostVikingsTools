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

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>

#include <SDL/SDL.h>

#include <liblv/lv_pack.h>
#include <liblv/lv_level.h>
#include <liblv/lv_sprite.h>
#include <liblv/lv_object_db.h>
#include <liblv/lv_debug.h>

#include "sdl_helpers.h"


#define TILE_WIDTH     8
#define TILE_HEIGHT    8
#define TILE_SIZE      (TILE_WIDTH * TILE_HEIGHT)

#define PREFAB_WIDTH   16
#define PREFAB_HEIGHT  16
#define PREFAB_SIZE    (PREFAB_WIDTH * PREFAB_HEIGHT)

#define COLOR_GRID         15
#define COLOR_OBJECT_BOX   13

static SDL_Surface *screen;

static bool draw_foreground_layer = true;
static bool draw_foreground       = true;
static bool draw_background       = true;
static bool draw_sky              = true;
static bool draw_objects          = true;
static bool draw_object_boxes     = true;
static bool draw_pal_animations   = false;

/* FIXME - hardcoded frames/palette offsets for the Vikings */
static const unsigned viking_idle_frames[] = { 0, 49, 0};
static const unsigned viking_fall_frames[] = {16, 39, 15};
static const unsigned viking_pal_base[]    = {0xf0, 0xb0, 0xf0};

static struct lv_pack pack;
static struct lv_level level;

static void rgb555_to_sdl_color(uint16_t color, SDL_Color *sdl_color)
{
    sdl_color->r = ((color >> 10) & 0x1f) << 2;
    sdl_color->g = ((color >>  5) & 0x1f) << 2;
    sdl_color->b = ((color >>  0) & 0x1f) << 2;
}

static void set_pal_color(SDL_Surface *surf, SDL_Color *color, unsigned index)
{
    SDL_SetPalette(surf, SDL_LOGPAL | SDL_PHYSPAL, color, index, 1);
}

static void get_pal_color(struct lv_level *level, unsigned index,
                          SDL_Color *color)
{
    color->r = level->palette[(index * 3) + 0] << 2;
    color->g = level->palette[(index * 3) + 1] << 2;
    color->b = level->palette[(index * 3) + 2] << 2;
}

static void update_palette_animation(SDL_Surface *surf, struct lv_level *level,
                                     struct lv_pal_animation *anim)
{
    SDL_Color color;
    size_t num_colors;
    int i, target;

    /*
     * If the two color indexes for the the palette animation are equal then
     * the animation is for a single color. A set of 16bit values encode the
     * colors to cycle through in RGB-555 format.
     *
     * If the two color indexes are equal then rotate the colors between the
     * upper and lower bound index.
     */
    if (anim->index1 == anim->index2) {
        anim->current_value += 1;
        anim->current_value %= anim->num_values;

        rgb555_to_sdl_color(anim->values[anim->current_value], &color);
        set_pal_color(surf, &color, anim->index1);

    } else if (anim->index1 < anim->index2) {
        num_colors = (anim->index2 - anim->index1) + 1;
        anim->current_value = (anim->current_value + 1) % num_colors;

        for (i = anim->index1; i <= anim->index2; i++) {
            target = i - anim->current_value;
            if (target < (int)anim->index1)
                target += num_colors;

            get_pal_color(level, target, &color);
            set_pal_color(surf, &color, i);
        }

    } else {
        num_colors = (anim->index1 - anim->index2) + 1;
        anim->current_value = (anim->current_value + 1) % num_colors;

        for (i = anim->index2; i <= anim->index1; i++) {
            target = i - anim->current_value;
            if (target < (int)anim->index2)
                target += num_colors;

            get_pal_color(level, target, &color);
            set_pal_color(surf, &color, i);
        }
    }
}

static void update_palette_animations(SDL_Surface *surf, struct lv_level *level)
{
    static unsigned last_update = 0;
    struct lv_pal_animation *anim;
    int i;

    if (last_update && SDL_GetTicks() - last_update < 50)
        return;

    last_update = SDL_GetTicks();

    for (i = 0; i < level->num_pal_animations; i++) {
        anim = &level->pal_animation[i];

        if (anim->counter == 0) {
            update_palette_animation(surf, level, anim);
            anim->counter = anim->max_counter;
        } else {
            anim->counter--;
        }
    }
}

static void draw_sprite32(struct lv_sprite_set *set, unsigned frame,
                          unsigned pal_base, SDL_Surface *dst,
                          unsigned x, unsigned y, unsigned flags)
{
    if (frame >= set->num_sprites)
        return;

    lv_sprite_draw_packed32(set->sprites[frame], pal_base,
			    (flags & LV_OBJ_FLAG_FLIP_HORIZ),
			    dst->pixels, x, y, dst->w);
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
    num_tiles = chunk->decompressed_size / TILE_SIZE;

    surf = sdl_create_surf(screen, num_tiles * TILE_WIDTH, TILE_HEIGHT);

    /* Render all the tiles in linear vga format on a single surface */
    SDL_LockSurface(surf);
    pixels = surf->pixels;
    for (i = 0; i < num_tiles; i++)
        lv_sprite_draw_raw(data + (i * TILE_SIZE), 0, TILE_WIDTH, TILE_HEIGHT,
			   false, false, pixels, i * TILE_WIDTH, 0, surf->w);
    SDL_UnlockSurface(surf);

    free(data);
    return surf;
}

static void draw_tile(SDL_Surface *surf, SDL_Surface *surf_tileset,
                      unsigned tile, unsigned flags, unsigned x, unsigned y)
{
    SDL_Rect src_rect, dst_rect;
    uint8_t base_color;

    if (!draw_foreground && (flags & LV_PREFAB_FLAG_FOREGROUND))
        return;
    if (!draw_background && !(flags & LV_PREFAB_FLAG_FOREGROUND))
        return;

    src_rect.x = tile * TILE_WIDTH;
    src_rect.y = 0;
    src_rect.w = TILE_WIDTH;
    src_rect.h = TILE_HEIGHT;

    dst_rect.x = x;
    dst_rect.y = y;
    dst_rect.w = TILE_WIDTH;
    dst_rect.h = TILE_HEIGHT;

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
              , x + TILE_WIDTH, y);
    draw_tile(surf, surf_tileset, prefab->tile[2], prefab->flags[2],
              x, y + TILE_HEIGHT);
    draw_tile(surf, surf_tileset, prefab->tile[3], prefab->flags[3],
              x + TILE_WIDTH, y + TILE_HEIGHT);
}

static SDL_Surface *create_level_surface(struct lv_level *level)
{
    return sdl_create_surf(screen, level->width * PREFAB_WIDTH,
                           level->height * PREFAB_HEIGHT);
}

static void draw_unpacked_sprite(SDL_Surface *surf, uint8_t *sprite,
                                 SDL_Rect *rect, bool flip)
{
    size_t tile_size, num_tiles;
    int i, x, y;

    tile_size = min(rect->w, rect->h);
    num_tiles = max(rect->w, rect->h) / tile_size;

    /*
     * FIXME - For multi-sprite objects (such as the hazard doors on level 1)
     * this will just draw the first tile repeated. Actual details for how
     * to draw the tiles are possibly managed by the progs in the object
     * database.
     */
    x = 0;
    y = 0;
    for (i = 0; i < num_tiles; i++) {
        lv_sprite_draw_unpacked(sprite, 0, tile_size, tile_size, flip, false,
				surf->pixels, rect->x + x, rect->y + y,
				surf->w);

        if (rect->h < rect->w)
            x += tile_size;
        else
            y += tile_size;
    }
}

static void draw_level_objects(SDL_Surface *surf)
{
    struct lv_object *obj;
    unsigned frame_set, frame;
    SDL_Rect r;
    int i;

    for (i = 0; i < level.num_objects; i++) {
        obj = &level.objects[i];

        if (obj->flags & LV_OBJ_FLAG_NO_DRAW)
            continue;

        r.x = obj->xoff - (obj->width / 2);
        r.y = obj->yoff - (obj->height / 2);
        r.w = obj->width;
        r.h = obj->height;

        if (obj->sprite_set) {
            switch (obj->sprite_set->format) {
            case LV_SPRITE_FORMAT_UNPACKED:
                r.x = obj->xoff - (obj->db_entry.width / 2);
                r.y = obj->yoff - (obj->db_entry.height / 2);
                r.w = obj->db_entry.width;
                r.h = obj->db_entry.height;

                if (obj->sprite_set && obj->sprite_set->num_sprites)
                    draw_unpacked_sprite(surf, obj->sprite_set->sprites[0],
                                         &r,
                                         obj->flags & LV_OBJ_FLAG_FLIP_HORIZ);
                break;
            }

        } else {
            switch (obj->type) {
            case LV_OBJ_ERIK:
            case LV_OBJ_BALEOG:
            case LV_OBJ_OLAF:
                if (obj->yoff > 0xff00) {
                    frame_set = obj->type + 3;
                    frame = viking_fall_frames[obj->type];
                    r.y = 0;
                } else {
                    frame_set = obj->type;
                    frame = viking_idle_frames[obj->type];
                }

                draw_sprite32(&level.sprite32_sets[frame_set], frame,
                              viking_pal_base[obj->type],
                              surf, r.x, r.y, obj->flags);
                break;

            default:
                break;
            }
        }

        /* Draw object bounding box */
        if (draw_object_boxes) {
            r.x = obj->xoff - (obj->width / 2);
            r.y = obj->yoff - (obj->height / 2);
            r.w = obj->width;
            r.h = obj->height;

            sdl_empty_box(surf, &r, COLOR_OBJECT_BOX);
        }
    }
}

static void draw_level(SDL_Surface *surf, SDL_Surface *surf_tileset)
{
    struct lv_tile_prefab *prefab;
    unsigned flags;
    int x, y;

    SDL_FillRect(surf, NULL, 0);

    /* Draw background/sky map (Blackthorne only) */
    if (draw_sky) {
        for (y = 0; y < level.height; y++) {
            for (x = 0; x < level.width; x++) {
                prefab = lv_level_get_bg_prefab_at(&level, x, y, NULL, &flags);
                if (prefab)
                    draw_prefab(surf, surf_tileset, prefab,
                                x * PREFAB_WIDTH, y * PREFAB_HEIGHT);
            }
        }
    }

    /* Draw foreground map */
    if (draw_foreground_layer) {
        for (y = 0; y < level.height; y++) {
            for (x = 0; x < level.width; x++) {
                prefab = lv_level_get_prefab_at(&level, x, y, NULL, &flags);
                if (prefab)
                    draw_prefab(surf, surf_tileset, prefab,
                                x * PREFAB_WIDTH, y * PREFAB_HEIGHT);
            }
        }
    }

    if (draw_objects)
        draw_level_objects(surf);
}

static void main_loop(SDL_Surface *surf_map, SDL_Surface *surf_tileset)
{
    unsigned xoff = 0, yoff = 0, tx, ty, tile, flags;
    int mouse_x = 0, mouse_y = 0, i;
    SDL_Rect rect;
    SDL_Event event;
    struct lv_tile_prefab *prefab;
    bool needs_redraw = true, draw_grid = false, done = false;

    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                done = true;
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    if (xoff >= PREFAB_WIDTH) {
                        xoff -= PREFAB_WIDTH;
                        needs_redraw = true;
                    }
                    break;

                case SDLK_RIGHT:
                    if (xoff < (level.width * PREFAB_WIDTH) - screen->w) {
                        xoff += PREFAB_WIDTH;
                        needs_redraw = true;
                    }
                    break;

                case SDLK_UP:
                    if (yoff >= PREFAB_HEIGHT) {
                        yoff -= PREFAB_HEIGHT;
                        needs_redraw = true;
                    }
                    break;

                case SDLK_DOWN:
                    if (yoff < (level.height * PREFAB_HEIGHT) - screen->h) {
                        yoff += PREFAB_HEIGHT;
                        needs_redraw = true;
                    }
                    break;

                case SDLK_s:
                    draw_sky = !draw_sky;
                    needs_redraw = true;
                    break;

                case SDLK_l:
                    draw_foreground_layer = !draw_foreground_layer;
                    needs_redraw = true;
                    break;

                case SDLK_f:
                    draw_foreground = !draw_foreground;
                    needs_redraw = true;
                    break;

                case SDLK_b:
                    draw_background = !draw_background;
                    needs_redraw = true;
                    break;

                case SDLK_o:
                    draw_objects = !draw_objects;
                    needs_redraw = true;
                    break;

                case SDLK_g:
                    draw_grid = !draw_grid;
                    needs_redraw = true;
                    break;

                case SDLK_r:
                    draw_object_boxes = !draw_object_boxes;
                    needs_redraw = true;
                    break;

                case SDLK_a:
                    draw_pal_animations = !draw_pal_animations;
                    break;

                default:
                    break;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                switch (event.button.button) {
                case SDL_BUTTON_LEFT:
                    SDL_GetMouseState(&mouse_x, &mouse_y);

                    tx = (mouse_x + xoff) / PREFAB_WIDTH;
                    ty = (mouse_y + yoff) / PREFAB_HEIGHT;

                    prefab = lv_level_get_prefab_at(&level, tx, ty,
                                                    &tile, &flags);

                    printf("Tile at %d, %d: %.4x, tile=%.2x, flags=%.2x\n",
                           tx, ty, level.map[(ty * level.width) + tx],
                           tile, flags);
                    for (i = 0; i < 4; i++)
                        printf("  [%.2x]: %.4x: %.4x\n",
                               i, prefab->tile[i], prefab->flags[i]);

                    /* Check if an object is in the clicked area */
                    tx = mouse_x + xoff;
                    ty = mouse_y + yoff;
                    for (i = 0; i < level.num_objects; i++) {
                        struct lv_object *obj = &level.objects[i];

                        if (tx >= obj->xoff - (obj->width / 2) &&
                            tx <= obj->xoff + (obj->width / 2) &&
                            ty >= obj->yoff - (obj->height / 2) &&
                            ty <= obj->yoff + (obj->height / 2)) {
                            printf("Object at %d, %d:\n", tx, ty);
                            printf("  type:    %.4x\n", obj->type);
                            printf("  xoff:    %d\n", obj->xoff);
                            printf("  yoff:    %d\n", obj->yoff);
                            printf("  size:    %dx%d\n",
                                   obj->width, obj->height);
                            printf("  size(c): %dx%d\n",
                                   obj->db_entry.width, obj->db_entry.height);
                            printf("  flags:   %.4x\n", obj->flags);
                            printf("  arg:     %.4x\n", obj->arg);
                            if (obj->sprite_set)
                                printf("  sprites: %d (%.4x)\n",
                                       obj->sprite_set->chunk_index,
                                       obj->sprite_set->chunk_index);

                        }
                    }
                    break;
                }
            }
        }

        if (draw_pal_animations)
            update_palette_animations(screen, &level);

        if (needs_redraw) {
            rect.x = xoff;
            rect.y = yoff;
            rect.w = screen->w;
            rect.h = screen->h;

            draw_level(surf_map, surf_tileset);

            if (draw_grid) {
                int x, y;

                for (x = 0; x < level.width; x++)
                    sdl_vline(surf_map, x * PREFAB_WIDTH, 0,
                               level.height * PREFAB_HEIGHT, COLOR_GRID);

                for (y = 0; y < level.height; y++)
                    sdl_hline(surf_map, 0, level.width * PREFAB_WIDTH,
                              y * PREFAB_HEIGHT, COLOR_GRID);
            }

            SDL_BlitSurface(surf_map, &rect, screen, NULL);
            SDL_Flip(screen);

            needs_redraw = false;
        }

        usleep(1);
    }
}

static void usage(const char *progname, int status)
{
    printf("Usage: %s [OPTIONS...] PACK_FILE LEVEL_NUM\n",
           progname);
    printf("\nOptions:\n");
    printf("  -B, --blackthorne            View Blackthorne levels\n");
    printf("  -d, --debug=FLAGS            Enable debugging\n");
    printf("  -h, --chunk-header=CHUNK     Level header chunk (overrides level)\n");
    printf("  -D, --chunk-object-db=CHUNK  Level object DB chunk (overrides level)\n");
    exit(status);
}

int main(int argc, char **argv)
{
    const struct option long_options[] = {
        {"blackthorne",     no_argument,       0, 'B'},
        {"debug",           required_argument, 0, 'd'},
        {"chunk-header",    no_argument,       0, 'h'},
        {"chunk-object-db", no_argument,       0, 'D'},
    };
    const char *short_options = "Bd:h:D:";
    const char *pack_filename;
    SDL_Surface *surf_tileset, *surf_map;
    unsigned debug_flags = 0, chunk_level_header = 0xffff,
        chunk_object_db = 0xffff, level_num;
    const struct lv_level_info *level_info;
    bool blackthorne = false;
    int c, option_index;

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

        case 'h':
            chunk_level_header = strtoul(optarg, NULL, 0);
            break;

        case 'D':
            chunk_object_db = strtoul(optarg, NULL, 0);
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
        printf("No pack file or level number specified\n");
        usage(argv[0], EXIT_FAILURE);
    }

    if (debug_flags)
        lv_debug_toggle(debug_flags);

    lv_pack_load(pack_filename, &pack, blackthorne);

    /*
     * Get the chunk indexes for this level. These are hardcoded in the
     * game binaries. Allow overriding the indexes.
     */
    level_info = lv_level_get_info(&pack, level_num);
    if (!level_info) {
        printf("Bad level number\n");
        exit(EXIT_FAILURE);
    }
    if (chunk_level_header == 0xffff)
        chunk_level_header = level_info->chunk_level_header;
    if (chunk_object_db == 0xffff)
        chunk_object_db = level_info->chunk_object_db;

    lv_level_load(&pack, &level, chunk_level_header, chunk_object_db);

    printf("%s level %d:\n",
           pack.blackthorne ? "Blackthorne" : "The Lost Vikings",
           level_num + 1);
    printf("    Chunk header:   %4d (%.4x)\n",
           chunk_level_header, chunk_level_header);
    printf("    Chunk objectdb: %4d (%.4x)\n",
           chunk_object_db, chunk_object_db);
    if (pack.blackthorne)
        printf("    Level size:     %dx%d (%dx%d rooms)\n",
               level.width, level.height, level.width / 16, level.height / 14);
    else
        printf("    Level size:     %dx%d\n", level.width, level.height);

    screen = sdl_init(640, 480);

    SDL_EnableKeyRepeat(250, 50);

    sdl_load_palette(screen, level.palette, 256);

    surf_tileset = load_tileset(level.chunk_tileset);

    surf_map = create_level_surface(&level);
    main_loop(surf_map, surf_tileset);

    exit(EXIT_SUCCESS);
}
