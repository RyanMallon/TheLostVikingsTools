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

#ifndef _BUFFER_H
#define _BUFFER_H

#include <stdint.h>
#include <endian.h>
#include <string.h>
#include <stdio.h>

struct buffer {
    void    *data;
    void    *p;
    size_t  size;
};

int buffer_init_from_file(struct buffer *buf, const char *filename);
void buffer_init_from_data(struct buffer *buf, void *data, size_t size);
int buffer_init(struct buffer *buf, size_t size);

static inline void buffer_seek(struct buffer *buf, unsigned offset)
{
    buf->p = buf->data + offset;
}

static inline unsigned buffer_offset(struct buffer *buf)
{
    return buf->p - buf->data;
}

static inline void buffer_get(struct buffer *buf, void *data, size_t size)
{
    memcpy(data, buf->p, size);
    buf->p += size;
}

static inline void buffer_get_u8(struct buffer *buf, uint8_t *val)
{
    buffer_get(buf, val, sizeof(*val));
}

static inline void buffer_peek(struct buffer *buf, off_t offset,
			       void *data, size_t size)
{
    memcpy(data, buf->data + offset, size);
}

static inline void buffer_peek_u8(struct buffer *buf, off_t offset, 
				  uint8_t *val)
{
    buffer_peek(buf, offset, val, sizeof(*val));
}

#define __DEFINE_BUFFER_GETTER(type, var_type)                  \
    static inline void buffer_get_##type(struct buffer *buf,    \
                                         var_type *val)         \
    {                                                           \
        buffer_get(buf, val, sizeof(*val));                     \
        type##toh(val);                                         \
    }

#define __DEFINE_BUFFER_PEEKER(type, var_type)                  \
    static inline void buffer_peek_##type(struct buffer *buf,   \
                                          off_t offset,         \
                                          var_type *val)        \
    {                                                           \
        buffer_peek(buf, offset, val, sizeof(*val));            \
        type##toh(val);                                         \
    }

__DEFINE_BUFFER_GETTER(le32, uint32_t);
__DEFINE_BUFFER_GETTER(le16, uint16_t);

__DEFINE_BUFFER_PEEKER(le32, uint32_t);
__DEFINE_BUFFER_PEEKER(le16, uint16_t);

#endif /* _BUFFER_H */
