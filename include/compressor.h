#ifndef HUFF_COMPRESSOR_H
#define HUFF_COMPRESSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "huffman.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool force_overwrite;
    bool verbose;
    bool benchmark;
} HuffCompressOptions;

typedef struct {
    uint64_t original_size;
    uint64_t compressed_size;
    int64_t  bytes_saved;
    double   compression_ratio;  /* (compressed_size / original_size) * 100.0 */
    double   space_saving;       /* (1.0 - compressed_size / original_size) * 100.0 */
    uint32_t crc32;
    int      distinct_symbols;
    double   elapsed_seconds;
    uint64_t freqs[HUFF_ALPHABET_SIZE];
    uint16_t code_lengths[HUFF_ALPHABET_SIZE];
} HuffCompressStats;

int  huff_compress_file(const char* in_path, const char* out_path, const HuffCompressOptions* options, HuffCompressStats* stats);
void huff_print_compress_stats(const HuffCompressStats* stats);
int  huff_print_file_info(const char* in_path);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_COMPRESSOR_H */
