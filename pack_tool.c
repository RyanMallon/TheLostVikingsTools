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
#include <getopt.h>

#include <liblv/lv_compress.h>
#include <liblv/lv_pack.h>
#include <liblv/buffer.h>
#include <liblv/common.h>

typedef void (*operation_func_t)(struct lv_chunk *chunk, const char *filename);

struct operation {
    operation_func_t  func;
    unsigned          chunk_index;
    const char        *filename;
    struct operation  *next;
};

static struct lv_pack pack;
static struct operation *operation_list;

static void fatal_error(const char *msg)
{
    printf("Fatal error: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void op_replace_compress(struct lv_chunk *chunk, const char *filename)
{
    struct buffer src_buf;
    size_t dst_max_size, dst_size;
    uint8_t *dst;
    int err;

    printf("Replacing compressed chunk %.4x with %s\n", chunk->index, filename);

    err = buffer_init_from_file(&src_buf, filename);
    if (err)
        fatal_error("Cannot open file for compressed replacement");

    dst_max_size = 2 * src_buf.size;
    dst = calloc(1, dst_max_size);
    if (!dst)
        fatal_error("Cannot allocate memory for compression buffer");

    /* Compressed chunks start with LE16 decompressed size minus 1 */
    dst[0] = (src_buf.size - 1);
    dst[1] = (src_buf.size - 1) >> 8;

    dst_size = lv_compress(src_buf.data, src_buf.size,
                           dst + 2, dst_max_size - 2);

    free(chunk->data);
    chunk->data = dst;
    chunk->size = dst_size;
}

static void op_extract_decompress(struct lv_chunk *chunk, const char *filename)
{
    uint8_t *data;
    FILE *fd;

    printf("Extracting and decompressing chunk %.4x to %s\n",
           chunk->index, filename);

    lv_decompress_chunk(chunk, &data);

    fd = fopen(filename, "w");
    if (!fd)
        fatal_error("Cannot open file for chunk decompression");

    fwrite(data, 1, chunk->decompressed_size, fd);
    fclose(fd);
    free(data);
}

static void op_extract_raw(struct lv_chunk *chunk, const char *filename)
{
    FILE *fd;

    printf("Extracting raw chunk %.4x to %s\n", chunk->index, filename);

    fd = fopen(filename, "w");
    if (!fd)
        fatal_error("Cannot open file for raw extract");

    fwrite(chunk->data, 1, chunk->size, fd);
    fclose(fd);
}

static void arg_get_chunk_and_filename(const char *arg, unsigned *chunk_index,
				       const char **filename)
{
    const char *p;

    p = strchr(arg, ':');
    if (!p)
        fatal_error("Bad argument format");

    *chunk_index = strtoul(arg, NULL, 0);
    *filename = p + 1;
}

static void do_operations(void)
{
    struct operation *op;
    struct lv_chunk *chunk;

    for (op = operation_list; op; op = op->next) {
        chunk = lv_pack_get_chunk(&pack, op->chunk_index);
        if (!chunk)
            fatal_error("Bad chunk index");

        op->func(chunk, op->filename);
    }
}

static void add_operation(operation_func_t func, unsigned chunk_index,
			  const char *filename)
{
    struct operation *op;

    op = calloc(1, sizeof(*op));
    if (!op)
        fatal_error("Cannot allocate memory for operation");

    op->func = func;
    op->chunk_index = chunk_index;
    op->filename = filename;

    op->next = operation_list;
    operation_list = op;
}

static void repack(const char *filename)
{
    uint32_t final;
    FILE *fd;
    int i;

    /* Recalculate chunk base offsets */
    for (i = 1; i < pack.num_chunks; i++)
        pack.chunks[i].start = pack.chunks[i - 1].start +
            pack.chunks[i - 1].size;

    fd = fopen(filename, "w");
    if (!fd)
        fatal_error("Cannot open file for repack");

    /* Write out the chunk headers */
    for (i = 0; i < pack.num_chunks; i++)
        fwrite(&pack.chunks[i].start, sizeof(pack.chunks[i].start), 1, fd);
    final = pack.chunks[i - 1].start + pack.chunks[i - 1].size;
    fwrite(&final, sizeof(final), 1, fd);

    /* Write out the chunk data */
    for (i = 0; i < pack.num_chunks; i++)
        fwrite(pack.chunks[i].data, pack.chunks[i].size, 1, fd);

    fclose(fd);
}

static int usage(const char *progname, int status)
{
    printf("Usage: %s [OPTIONS...] DATA_FILE\n", progname);
    printf("\nOptions:\n");
    printf("  -B, --blackthorne                     Pack file is Blackthorne format\n");
    printf("  -l, --list-chunks                     List chunks in data file\n");
    printf("  -r, --replace-chunk=CHUNK:FILENAME    Replace a chunk\n");
    printf("  -e, --extract-chunk=CHUNK:FILENAME    Extract raw chunk\n");
    printf("  -d, --decompress-chunk=CHUNK:FILENAME Decompress and extract chunk\n");
    printf("  -o, --output-file=FILENAME            Output file to write to for repacking\n");
    printf("  -?, --help                            Help\n");

    exit(status);
}

/*
 * Examples:
 *
 * List all of the chunks in the pack file:
 *   ./pack_tool DATA.DAT -l
 *
 * Extract and decompress chunk 4 (Erik HUD image, raw 32x24)
 *   ./pack_tool DATA.DAT -d4:erik.img
 *
 * Replace chunk 4 and create a new pack file:
 *
 *   ./pack_tool DATA.DAT -r4:erik_new.img -o DATA_NEW.DAT
 *
 */
int main(int argc, char **argv)
{
    const struct option long_options[] = {
        {"blackthorne",       no_argument,       0, 'B'},
        {"list-chunks",       no_argument,       0, 'l'},
        {"extract-raw-chunk", required_argument, 0, 'e'},
        {"decompress-chunk",  required_argument, 0, 'd'},
        {"replace-chunk",     required_argument, 0, 'r'},
        {"output-file",       required_argument, 0, 'o'},
        {"help",              no_argument,       0, '?'},
        {NULL, 0, 0, 0},
    };
    const char *short_options = "Ble:d:r:o:?";
    struct lv_chunk *chunk;
    unsigned chunk_index;
    int i, option_index, c;
    bool blackthorne = false, list_chunks = false, needs_repack = false;
    const char *filename, *data_file = NULL, *outfile = NULL;

    while (1) {
        c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'B':
            blackthorne = true;
            break;

        case 'l':
            list_chunks = true;
            break;

        case 'r':
            arg_get_chunk_and_filename(optarg, &chunk_index, &filename);
            add_operation(op_replace_compress, chunk_index, filename);
            needs_repack = true;
            break;

        case 'e':
            arg_get_chunk_and_filename(optarg, &chunk_index, &filename);
            add_operation(op_extract_raw, chunk_index, filename);
            break;

        case 'd':
            arg_get_chunk_and_filename(optarg, &chunk_index, &filename);
            add_operation(op_extract_decompress, chunk_index, filename);
            break;

        case 'o':
            outfile = optarg;
            break;

        case '?':
            usage(argv[0], EXIT_SUCCESS);
            break;

        default:
            printf("Unknown argument %c\n", c);
            usage(argv[0], EXIT_FAILURE);
            break;
        }
    }

    if (optind == argc - 1) {
        data_file = argv[optind];
    } else {
        printf("No data file specified\n");
        usage(argv[0], EXIT_FAILURE);
    }

    lv_pack_load(data_file, &pack, blackthorne);

    if (list_chunks) {
        printf("%zd chunks:\n", pack.num_chunks);
        for (i = 0; i < pack.num_chunks; i++) {
            chunk = &pack.chunks[i];

            printf("  [%.4d] start=%.6x, size=%.4zx, decompressed_size=%.4zx\n",
                   i, chunk->start, chunk->size, chunk->decompressed_size);
        }
    }

    do_operations();
    if (needs_repack)
        repack(outfile);

    exit(EXIT_SUCCESS);
}
