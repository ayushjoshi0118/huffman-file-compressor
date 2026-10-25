#include "file_format.h"
#include "utils.h"
#include <string.h>

int huff_validate_header(const HuffHeader* header) {
    if (!header) return HUFF_ERR_ARGS;

    if (header->magic != HUFF_MAGIC_VAL) {
        return HUFF_ERR_INVALID_MAGIC;
    }

    if (header->version != HUFF_VERSION_1) {
        return HUFF_ERR_UNSUPPORTED_VERSION;
    }

    /* Reserved field must currently be zero */
    if (header->reserved != 0) {
        return HUFF_ERR_CORRUPT_HEADER;
    }

    /* Flags validation */
    bool is_empty = (header->flags & HUFF_FLAG_EMPTY_FILE) != 0;
    bool is_single = (header->flags & HUFF_FLAG_SINGLE_SYMBOL) != 0;

    if (is_empty && is_single) {
        return HUFF_ERR_CORRUPT_HEADER;
    }

    if (is_empty && header->original_size != 0) {
        return HUFF_ERR_CORRUPT_HEADER;
    }

    if (!is_empty && header->original_size == 0) {
        return HUFF_ERR_CORRUPT_HEADER;
    }

    if (is_single && header->original_size == 0) {
        return HUFF_ERR_CORRUPT_HEADER;
    }

    return HUFF_OK;
}

int huff_write_header(FILE* fp, const HuffHeader* header) {
    if (!fp || !header) return HUFF_ERR_ARGS;

    int valid = huff_validate_header(header);
    if (valid != HUFF_OK) return valid;

    uint8_t buffer[HUFF_HEADER_BASE_SIZE + 1];
    huff_write_u32_be(buffer + 0, header->magic);
    buffer[4] = header->version;
    buffer[5] = header->flags;
    huff_write_u16_be(buffer + 6, header->reserved);
    huff_write_u64_be(buffer + 8, header->original_size);
    huff_write_u32_be(buffer + 16, header->crc32);

    size_t to_write = HUFF_HEADER_BASE_SIZE;
    if (header->flags & HUFF_FLAG_SINGLE_SYMBOL) {
        buffer[20] = header->single_symbol;
        to_write += 1;
    }

    size_t written = fwrite(buffer, 1, to_write, fp);
    if (written != to_write) {
        return HUFF_ERR_IO;
    }

    return HUFF_OK;
}

int huff_read_header(FILE* fp, HuffHeader* header) {
    if (!fp || !header) return HUFF_ERR_ARGS;

    memset(header, 0, sizeof(HuffHeader));

    uint8_t buffer[HUFF_HEADER_BASE_SIZE];
    size_t n = fread(buffer, 1, HUFF_HEADER_BASE_SIZE, fp);
    if (n < HUFF_HEADER_BASE_SIZE) {
        if (n == 0 && feof(fp)) {
            return HUFF_ERR_PREMATURE_EOF;
        }
        return HUFF_ERR_CORRUPT_HEADER;
    }

    header->magic = huff_read_u32_be(buffer + 0);
    header->version = buffer[4];
    header->flags = buffer[5];
    header->reserved = huff_read_u16_be(buffer + 6);
    header->original_size = huff_read_u64_be(buffer + 8);
    header->crc32 = huff_read_u32_be(buffer + 16);

    if (header->flags & HUFF_FLAG_SINGLE_SYMBOL) {
        int c = fgetc(fp);
        if (c == EOF) {
            return HUFF_ERR_CORRUPT_HEADER;
        }
        header->single_symbol = (uint8_t)c;
    }

    return huff_validate_header(header);
}
