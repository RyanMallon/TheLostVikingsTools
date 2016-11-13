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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "buffer.h"

int buffer_init(struct buffer *buf, size_t size)
{
	buf->size = size;
	buf->data = malloc(buf->size);
	if (!buf->size)
		return -ENOMEM;

	buf->p = buf->data;
	return 0;
}

void buffer_init_from_data(struct buffer *buf, void *data, size_t size)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->size = size;
	buf->p = buf->data;
}

int buffer_init_from_file(struct buffer *buf, const char *filename)
{
	struct stat s;
	FILE *fd;
	int err;

	fd = fopen(filename, "r");
	if (!fd)
		return -errno;

	fstat(fileno(fd), &s);

	err = buffer_init(buf, s.st_size);
	if (err) {
		fclose(fd);
		return err;
	}

	fread(buf->data, 1, buf->size, fd);
	fclose(fd);
	return 0;
}
