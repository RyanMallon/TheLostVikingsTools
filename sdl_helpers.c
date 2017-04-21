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
#include <stdint.h>
#include <stdlib.h>

#include <SDL/SDL.h>

static SDL_Color sdl_pal[256];

static void swap(unsigned *a, unsigned *b)
{
    unsigned temp = *a;

    *a = *b;
    *b = temp;
}

static void draw_pixel(SDL_Surface *surf, unsigned x, unsigned y,
		       unsigned color)
{
    uint8_t *pixels = surf->pixels;

    if (x < surf->w && y < surf->h)
        pixels[(y * surf->w) + x] = color;
}

void sdl_load_palette(SDL_Surface *surf, uint8_t *pal, size_t num_colors)
{
    int i;

    memset(sdl_pal, 0, sizeof(sdl_pal));
    for (i = 0; i < num_colors; i++) {
        sdl_pal[i].r = pal[(i * 3) + 0] << 2;
        sdl_pal[i].g = pal[(i * 3) + 1] << 2;
        sdl_pal[i].b = pal[(i * 3) + 2] << 2;
    }

    SDL_SetPalette(surf, SDL_LOGPAL | SDL_PHYSPAL, sdl_pal, 0, 256);
}

struct SDL_Surface *sdl_create_surf(SDL_Surface *parent,
                                    size_t width, size_t height)
{
    SDL_Surface *surf;

    surf = SDL_CreateRGBSurface(parent->flags, width, height,
                                parent->format->BitsPerPixel,
                                parent->format->Rmask,
                                parent->format->Gmask,
                                parent->format->Bmask,
                                parent->format->Amask);
    SDL_SetPalette(surf, SDL_LOGPAL | SDL_PHYSPAL, sdl_pal, 0, 256);
    return surf;
}

void sdl_blit(SDL_Surface *src, SDL_Rect *src_rect,
              SDL_Surface *dst, SDL_Rect *dst_rect,
              uint8_t base_color, bool flip_horiz, bool flip_vert)
{
    uint8_t *src_pixels, *dst_pixels, pixel;
    unsigned x, y, src_base, dst_base;

    dst_pixels = dst->pixels;
    src_pixels = src->pixels;

    for (y = 0; y < src_rect->h; y++) {
        src_base = ((y + src_rect->y) * src->w) + src_rect->x;
        dst_base = ((y + dst_rect->y) * dst->w) + dst_rect->x;

        if (flip_vert)
            dst_base = (((dst_rect->h - y - 1) + dst_rect->y) * dst->w) + dst_rect->x;
        else
            dst_base = ((y + dst_rect->y) * dst->w) + dst_rect->x;

        for (x = 0; x < src_rect->w; x++) {
            pixel = src_pixels[src_base + x] + base_color;
            if (flip_horiz)
                dst_pixels[dst_base + dst_rect->w - x - 1] = pixel;
            else
                dst_pixels[dst_base + x] = pixel;
        }
    }
}

void sdl_draw_line(SDL_Surface *surf, unsigned x1, unsigned y1,
                   unsigned x2, unsigned y2, unsigned color)
{
    unsigned dx, dy, x, y, e, steep;

    steep = abs(y2 - y1) > abs(x2 - x1);
    if (steep) {
        swap(&x1, &y1);
        swap(&x2, &y2);
    }

    if (x1 > x2) {
        swap(&x1, &x2);
        swap(&y1, &y2);
    }

    dx = abs(x2 - x1);
    dy = abs(y2 - y1);
    e = 0;

    for (x = x1, y = y1; x <= x2; x++) {
        if (steep)
            draw_pixel(surf, y, x, color);
        else
            draw_pixel(surf, x, y, color);

        if ((e + dy) << 1 < dx) {
            e = e + dy;
        } else {
            if (y1 < y2)
                y++;
            else
                y--;
            e = e + dy - dx;
        }
    }
}

void sdl_vline(SDL_Surface *surf, unsigned x, unsigned y1,
               unsigned y2, unsigned color)
{
    SDL_Rect rect;

    rect.x = x;
    rect.y = y1;
    rect.w = 1;
    rect.h = y2 - y1;

    SDL_FillRect(surf, &rect, color);
}

void sdl_hline(SDL_Surface *surf, unsigned x1, unsigned x2,
               unsigned y, unsigned color)
{
    SDL_Rect rect;

    rect.x = x1;
    rect.y = y;
    rect.w = x2 - x1;
    rect.h = 1;

    SDL_FillRect(surf, &rect, color);
}

void sdl_empty_box(SDL_Surface *surf, SDL_Rect *r, unsigned color)
{
    sdl_vline(surf, r->x, r->y, r->y + r->h, color);
    sdl_vline(surf, r->x + r->w, r->y, r->y + r->h, color);
    sdl_hline(surf, r->x, r->x + r->w, r->y, color);
    sdl_hline(surf, r->x, r->x + r->w, r->y + r->h, color);
}

SDL_Surface *sdl_init(unsigned width, unsigned height)
{
    SDL_Init(SDL_INIT_VIDEO);
    return SDL_SetVideoMode(width, height, 8, (SDL_HWSURFACE |
                                               SDL_HWPALETTE |
                                               SDL_DOUBLEBUF));
}
