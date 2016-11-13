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

#ifndef _LV_COMMON_H
#define _LV_COMMON_H

#ifndef __packed
#define __packed __attribute((packed))
#endif

#ifndef min
#define min(x, y) ({x < y ? x : y;})
#endif

#ifndef max
#define max(x, y) ({x > y ? x : y;})
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#endif /* _LV_COMMON_H */
