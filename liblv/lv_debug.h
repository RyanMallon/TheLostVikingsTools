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

#ifndef _LV_DEBUG_H
#define _LV_DEBUG_H

#define LV_DEBUG_LEVEL   (1 << 0)

void lv_debug_toggle(unsigned flags);
void lv_debug(unsigned level, const char *fmt, ...);

#endif /* _LV_DEBUG_H */
