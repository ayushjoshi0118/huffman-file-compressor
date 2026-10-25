#ifndef HUFF_DECOMPRESSOR_H
#define HUFF_DECOMPRESSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool force_overwrite;
    bool verbose;
    bool benchmark;
} HuffDecompressOptions;

typedef struct {
    uint64_t original_size;
    uint64_t compressed_size;
    uint32_t expected_crc32;
    uint32_t actual_crc32;
    double   elapsed_seconds;
} HuffDecompressStats;

int  huff_decompress_file(const char* in_path, const char* out_path, const HuffDecompressOptions* options, HuffDecompressStats* stats);
void huff_print_decompress_stats(const HuffDecompressStats* stats);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_DECOMPRESSOR_H */
