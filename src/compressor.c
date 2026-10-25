#include "compressor.h"
#include "file_format.h"
#include "checksum.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

int huff_compress_file(const char* in_path, const char* out_path,
                       const HuffCompressOptions* options, HuffCompressStats* stats) {
    if (!in_path || !out_path) return HUFF_ERR_ARGS;

    if (huff_is_same_file(in_path, out_path)) {
        return HUFF_ERR_SAME_FILE;
    }

    bool force = options ? options->force_overwrite : false;
    if (huff_file_exists(out_path) && !force) {
        return HUFF_ERR_OUTPUT_EXISTS;
    }

    clock_t start_time = clock();

    /* Pass 1: Frequency Analysis */
    FILE* in_fp = fopen(in_path, "rb");
    if (!in_fp) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }

    uint64_t freqs[HUFF_ALPHABET_SIZE];
    uint64_t original_size = 0;
    uint32_t crc32_val = 0;

    int freq_res = huff_count_frequencies(in_fp, freqs, &original_size, &crc32_val);
    fclose(in_fp);
    in_fp = NULL;

    if (freq_res != HUFF_OK) {
        return freq_res;
    }

    int distinct_count = 0;
    for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
        if (freqs[i] > 0) distinct_count++;
    }

    /* Prepare Header */
    HuffHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = HUFF_MAGIC_VAL;
    header.version = HUFF_VERSION_1;
    header.flags = HUFF_FLAG_HAS_CRC32;
    header.reserved = 0;
    header.original_size = original_size;
    header.crc32 = crc32_val;

    if (original_size == 0) {
        header.flags |= HUFF_FLAG_EMPTY_FILE;
    } else if (distinct_count == 1) {
        header.flags |= HUFF_FLAG_SINGLE_SYMBOL;
        for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
            if (freqs[i] > 0) {
                header.single_symbol = (uint8_t)i;
                break;
            }
        }
    }

    /* Open temporary file for writing */
    char temp_path[1024];
    if (!huff_create_temp_path(out_path, temp_path, sizeof(temp_path))) {
        return HUFF_ERR_IO;
    }

    FILE* out_fp = fopen(temp_path, "wb");
    if (!out_fp) {
        return HUFF_ERR_IO;
    }

    int hdr_res = huff_write_header(out_fp, &header);
    if (hdr_res != HUFF_OK) {
        fclose(out_fp);
        remove(temp_path);
        return hdr_res;
    }

    HuffmanNode* root = NULL;
    HuffmanCodeTable code_table;
    memset(&code_table, 0, sizeof(code_table));

    if (original_size > 0 && distinct_count >= 2) {
        /* Construct Huffman Tree and Code Table */
        int built_distinct = 0;
        root = huffman_build_tree(freqs, &built_distinct);
        if (!root) {
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_MEMORY;
        }

        int code_res = huffman_generate_codes(root, &code_table);
        if (code_res != HUFF_OK) {
            huffman_free_tree(root);
            fclose(out_fp);
            remove(temp_path);
            return code_res;
        }

        /* Initialize BitWriter on output file */
        BitWriter bw;
        if (bitwriter_init(&bw, out_fp) != HUFF_OK) {
            huffman_free_tree(root);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_IO;
        }

        /* Serialize the Huffman tree */
        int ser_res = huffman_serialize_tree(root, &bw);
        if (ser_res != HUFF_OK) {
            huffman_free_tree(root);
            fclose(out_fp);
            remove(temp_path);
            return ser_res;
        }

        /* Pass 2: Encode Payload */
        in_fp = fopen(in_path, "rb");
        if (!in_fp) {
            huffman_free_tree(root);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_FILE_NOT_FOUND;
        }

        uint8_t buffer[HUFF_BUFFER_SIZE];
        size_t bytes_read = 0;
        uint64_t pass2_bytes = 0;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), in_fp)) > 0) {
            for (size_t i = 0; i < bytes_read; i++) {
                uint8_t sym = buffer[i];
                HuffmanCode* code = &code_table.codes[sym];
                int wres = bitwriter_write_code(&bw, code->bits, code->length);
                if (wres != HUFF_OK) {
                    fclose(in_fp);
                    huffman_free_tree(root);
                    fclose(out_fp);
                    remove(temp_path);
                    return wres;
                }
            }
            pass2_bytes += bytes_read;
        }

        fclose(in_fp);
        in_fp = NULL;

        if (pass2_bytes != original_size) {
            huffman_free_tree(root);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_IO;
        }

        /* Flush BitWriter */
        int flush_res = bitwriter_flush(&bw);
        huffman_free_tree(root);
        root = NULL;

        if (flush_res != HUFF_OK) {
            fclose(out_fp);
            remove(temp_path);
            return flush_res;
        }
    }

    if (fclose(out_fp) != 0) {
        remove(temp_path);
        return HUFF_ERR_IO;
    }

    /* Atomic replace */
    if (huff_file_exists(out_path)) {
        remove(out_path);
    }
    if (rename(temp_path, out_path) != 0) {
        remove(temp_path);
        return HUFF_ERR_IO;
    }

    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / (double)CLOCKS_PER_SEC;

    /* Fill statistics */
    uint64_t comp_size = 0;
    huff_get_file_size(out_path, &comp_size);

    if (stats) {
        stats->original_size = original_size;
        stats->compressed_size = comp_size;
        stats->bytes_saved = (int64_t)original_size - (int64_t)comp_size;
        stats->compression_ratio = (original_size > 0) ?
            ((double)comp_size / (double)original_size) * 100.0 : 0.0;
        stats->space_saving = (original_size > 0) ?
            (1.0 - (double)comp_size / (double)original_size) * 100.0 : 0.0;
        stats->crc32 = crc32_val;
        stats->distinct_symbols = distinct_count;
        stats->elapsed_seconds = elapsed;
        memcpy(stats->freqs, freqs, sizeof(freqs));
        for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
            stats->code_lengths[i] = code_table.codes[i].length;
        }
    }

    return HUFF_OK;
}

void huff_print_compress_stats(const HuffCompressStats* stats) {
    if (!stats) return;

    printf("\n--- Huffman Compression Statistics ---\n");
    printf("Original size:     %llu bytes\n", (unsigned long long)stats->original_size);
    printf("Compressed size:   %llu bytes (including metadata)\n", (unsigned long long)stats->compressed_size);
    printf("Distinct symbols:  %d of 256\n", stats->distinct_symbols);
    printf("Data CRC-32:       0x%08X\n", stats->crc32);

    if (stats->original_size > 0) {
        printf("Compression ratio: %.2f%% (compressed / original)\n", stats->compression_ratio);
        printf("Space savings:     %.2f%%\n", stats->space_saving);
        if (stats->bytes_saved > 0) {
            printf("Result:            Saved %lld bytes (file is smaller)\n", (long long)stats->bytes_saved);
        } else if (stats->bytes_saved < 0) {
            printf("Result:            Grew by %lld bytes (file is larger)\n", (long long)(-stats->bytes_saved));
            printf("Note: Huffman headers introduce overhead. For small or high-entropy files,\n"
                   "the header and tree metadata can exceed the savings from bit coding.\n");
        } else {
            printf("Result:            Exact same size as original\n");
        }
    } else {
        printf("Result:            Empty file compressed successfully (header-only)\n");
    }

    if (stats->elapsed_seconds >= 0.0) {
        printf("Elapsed time:      %.4f seconds\n", stats->elapsed_seconds);
    }
    printf("---------------------------------------\n\n");
}

int huff_print_file_info(const char* in_path) {
    if (!in_path) return HUFF_ERR_ARGS;

    FILE* fp = fopen(in_path, "rb");
    if (!fp) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }

    uint64_t freqs[HUFF_ALPHABET_SIZE];
    uint64_t total_bytes = 0;
    uint32_t crc32_val = 0;

    int res = huff_count_frequencies(fp, freqs, &total_bytes, &crc32_val);
    fclose(fp);

    if (res != HUFF_OK) {
        return res;
    }

    int distinct_count = 0;
    double entropy = 0.0;

    for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
        if (freqs[i] > 0) {
            distinct_count++;
            double p = (double)freqs[i] / (double)total_bytes;
            entropy -= p * (log(p) / log(2.0));
        }
    }

    printf("\n=== File Information & Frequency Analysis ===\n");
    printf("File path:         %s\n", in_path);
    printf("Total size:        %llu bytes\n", (unsigned long long)total_bytes);
    printf("Distinct symbols:  %d / 256\n", distinct_count);
    printf("Shannon entropy:   %.4f bits/byte (theoretical min: %.2f KB)\n",
           entropy, (total_bytes * entropy / 8.0) / 1024.0);
    printf("CRC-32 checksum:   0x%08X\n\n", crc32_val);

    printf("Top byte frequencies:\n");
    printf("%-8s %-12s %-12s %-10s\n", "Symbol", "Hex / Char", "Count", "Frequency");
    printf("---------------------------------------------\n");

    /* Sort symbols by frequency descending */
    int indices[HUFF_ALPHABET_SIZE];
    for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) indices[i] = i;

    for (int i = 0; i < HUFF_ALPHABET_SIZE - 1; i++) {
        for (int j = i + 1; j < HUFF_ALPHABET_SIZE; j++) {
            if (freqs[indices[j]] > freqs[indices[i]]) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    int printed = 0;
    for (int i = 0; i < HUFF_ALPHABET_SIZE && printed < 16; i++) {
        int sym = indices[i];
        if (freqs[sym] == 0) break;
        double pct = (total_bytes > 0) ? ((double)freqs[sym] / (double)total_bytes) * 100.0 : 0.0;
        char char_repr[16];
        if (sym >= 32 && sym <= 126) {
            snprintf(char_repr, sizeof(char_repr), "'%c' (0x%02X)", (char)sym, sym);
        } else {
            snprintf(char_repr, sizeof(char_repr), "0x%02X", sym);
        }
        printf("%-8d %-12s %-12llu %6.2f%%\n", sym, char_repr, (unsigned long long)freqs[sym], pct);
        printed++;
    }

    if (distinct_count > 16) {
        printf("... and %d more distinct symbols.\n", distinct_count - 16);
    }
    printf("=============================================\n\n");

    return HUFF_OK;
}
