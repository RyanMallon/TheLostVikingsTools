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

#ifndef _SDL_HELPERS_H
#define _SDL_HELPERS_H

#include <stdbool.h>
#include <stdio.h>

#include <SDL/SDL.h>

void sdl_load_palette(SDL_Surface *surf, uint8_t *pal, size_t num_colors);
struct SDL_Surface *sdl_create_surf(SDL_Surface *parent,
				    size_t width, size_t height);
void sdl_blit_flip(SDL_Surface *src, SDL_Rect *src_rect,
                   SDL_Surface *dst, SDL_Rect *dst_rect,
                   bool flip_horiz, bool flip_vert);
void sdl_draw_line(SDL_Surface *surf, unsigned x1, unsigned y1,
                   unsigned x2, unsigned y2, unsigned color);
void sdl_vline(SDL_Surface *surf, unsigned x, unsigned y1,
               unsigned y2, unsigned color);
void sdl_hline(SDL_Surface *surf, unsigned x1, unsigned x2,
               unsigned y, unsigned color);
void sdl_empty_box(SDL_Surface *surf, SDL_Rect *r, unsigned color);

#endif /* _SDL_HELPERS_H */
