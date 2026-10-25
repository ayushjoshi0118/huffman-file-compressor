#ifndef HUFF_FILE_FORMAT_H
#define HUFF_FILE_FORMAT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUFF_MAGIC_STRING "HUFF"
#define HUFF_MAGIC_VAL    0x48554646U /* 'H' 'U' 'F' 'F' */
#define HUFF_VERSION_1    1

/* Header flags */
#define HUFF_FLAG_EMPTY_FILE    0x01
#define HUFF_FLAG_SINGLE_SYMBOL 0x02
#define HUFF_FLAG_HAS_CRC32     0x04

/* Fixed header size in bytes */
#define HUFF_HEADER_BASE_SIZE   20

typedef struct {
    uint32_t magic;
    uint8_t  version;
    uint8_t  flags;
    uint16_t reserved;
    uint64_t original_size;
    uint32_t crc32;
    uint8_t  single_symbol; /* Valid only if HUFF_FLAG_SINGLE_SYMBOL is set */
} HuffHeader;

int huff_write_header(FILE* fp, const HuffHeader* header);
int huff_read_header(FILE* fp, HuffHeader* header);
int huff_validate_header(const HuffHeader* header);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_FILE_FORMAT_H */
