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

#ifndef _LV_COMPRESS_H
#define _LV_COMPRESS_H

#include <stdint.h>
#include <stdio.h>

/**
 * \defgroup lv_compress Compression/Decompression
 * \{
 *
 * Lost Vikings uses a variant of the LZSS compression algorithm with a
 * 4K sliding window. Most of the chunks in the data file are compressed
 * using this scheme.
 */

/**
 * Compress data using the LZSS compression scheme.
 *
 * \param src       Source data to compress.
 * \param src_size  Size of the source data to compress.
 * \param dst       Destination buffer to write compressed data to.
 * \param dst_size  Size of the destination buffer.
 * \returns         Size of the compressed data.
 */
size_t lv_compress(const uint8_t *src, size_t src_size,
		   uint8_t *dst, size_t dst_size);

/**
 * Decompress LZSS encoded data.
 *
 * \param src      Compressed source data.
 * \param src_size Size of the compressed source data.
 * \param dst      Destination buffer to decompress into.
 * \param dst_size Size of the destination buffer.
 */
void lv_decompress(const uint8_t *src, size_t src_size,
		   uint8_t *dst, size_t dst_size);

/** \} */

#endif /* _LV_COMPRESS_H */
