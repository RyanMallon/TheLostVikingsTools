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

#include <stdarg.h>
#include <stdio.h>

#include "lv_debug.h"

static unsigned debug_flags;

void lv_debug(unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (debug_flags & level)
        vprintf(fmt, args);
    va_end(args);
}

void lv_debug_toggle(unsigned flags)
{
    debug_flags = flags;
}
