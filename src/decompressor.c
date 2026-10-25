#include "decompressor.h"
#include "file_format.h"
#include "huffman.h"
#include "bitstream.h"
#include "checksum.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int huff_decompress_file(const char* in_path, const char* out_path,
                         const HuffDecompressOptions* options, HuffDecompressStats* stats) {
    if (!in_path || !out_path) return HUFF_ERR_ARGS;

    if (huff_is_same_file(in_path, out_path)) {
        return HUFF_ERR_SAME_FILE;
    }

    bool force = options ? options->force_overwrite : false;
    if (huff_file_exists(out_path) && !force) {
        return HUFF_ERR_OUTPUT_EXISTS;
    }

    clock_t start_time = clock();

    FILE* in_fp = fopen(in_path, "rb");
    if (!in_fp) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }

    HuffHeader header;
    int hdr_res = huff_read_header(in_fp, &header);
    if (hdr_res != HUFF_OK) {
        fclose(in_fp);
        return hdr_res;
    }

    char temp_path[1024];
    if (!huff_create_temp_path(out_path, temp_path, sizeof(temp_path))) {
        fclose(in_fp);
        return HUFF_ERR_IO;
    }

    FILE* out_fp = fopen(temp_path, "wb");
    if (!out_fp) {
        fclose(in_fp);
        return HUFF_ERR_IO;
    }

    uint32_t computed_crc = 0;

    /* Case A: Empty file */
    if (header.flags & HUFF_FLAG_EMPTY_FILE) {
        computed_crc = huff_crc32_compute(NULL, 0);
        if (computed_crc != header.crc32) {
            fclose(in_fp);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_CRC_MISMATCH;
        }
    }
    /* Case B: Single unique symbol */
    else if (header.flags & HUFF_FLAG_SINGLE_SYMBOL) {
        uint8_t buffer[HUFF_BUFFER_SIZE];
        memset(buffer, header.single_symbol, sizeof(buffer));

        uint64_t remaining = header.original_size;
        uint32_t crc = huff_crc32_init();

        while (remaining > 0) {
            size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : (size_t)remaining;
            if (fwrite(buffer, 1, chunk, out_fp) != chunk) {
                fclose(in_fp);
                fclose(out_fp);
                remove(temp_path);
                return HUFF_ERR_IO;
            }
            crc = huff_crc32_update(crc, buffer, chunk);
            remaining -= chunk;
        }

        computed_crc = huff_crc32_finalize(crc);
        if (computed_crc != header.crc32) {
            fclose(in_fp);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_CRC_MISMATCH;
        }
    }
    /* Case C: Normal multi-symbol compressed stream */
    else {
        BitReader br;
        if (bitreader_init(&br, in_fp) != HUFF_OK) {
            fclose(in_fp);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_IO;
        }

        int tree_err = HUFF_OK;
        HuffmanNode* root = huffman_deserialize_tree(&br, &tree_err);
        if (!root) {
            fclose(in_fp);
            fclose(out_fp);
            remove(temp_path);
            return (tree_err != HUFF_OK) ? tree_err : HUFF_ERR_CORRUPT_TREE;
        }

        uint8_t out_buf[HUFF_BUFFER_SIZE];
        size_t out_pos = 0;
        uint64_t bytes_decoded = 0;
        uint32_t crc = huff_crc32_init();
        HuffmanNode* curr = root;

        if (root->is_leaf) {
            /* Single-leaf tree encoded in bitstream: 1 bit per symbol */
            while (bytes_decoded < header.original_size) {
                int bit = bitreader_read_bit(&br);
                if (bit < 0) {
                    huffman_free_tree(root);
                    fclose(in_fp);
                    fclose(out_fp);
                    remove(temp_path);
                    return HUFF_ERR_PREMATURE_EOF;
                }

                out_buf[out_pos++] = root->symbol;
                bytes_decoded++;

                if (out_pos == sizeof(out_buf)) {
                    crc = huff_crc32_update(crc, out_buf, out_pos);
                    if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) {
                        huffman_free_tree(root);
                        fclose(in_fp);
                        fclose(out_fp);
                        remove(temp_path);
                        return HUFF_ERR_IO;
                    }
                    out_pos = 0;
                }
            }
        } else {
            /* Normal multi-node tree traversal */
            while (bytes_decoded < header.original_size) {
                int bit = bitreader_read_bit(&br);
                if (bit < 0) {
                    huffman_free_tree(root);
                    fclose(in_fp);
                    fclose(out_fp);
                    remove(temp_path);
                    return HUFF_ERR_PREMATURE_EOF;
                }

                curr = (bit == 0) ? curr->left : curr->right;
                if (!curr) {
                    huffman_free_tree(root);
                    fclose(in_fp);
                    fclose(out_fp);
                    remove(temp_path);
                    return HUFF_ERR_CORRUPT_PAYLOAD;
                }

                if (curr->is_leaf) {
                    out_buf[out_pos++] = curr->symbol;
                    bytes_decoded++;

                    if (out_pos == sizeof(out_buf)) {
                        crc = huff_crc32_update(crc, out_buf, out_pos);
                        if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) {
                            huffman_free_tree(root);
                            fclose(in_fp);
                            fclose(out_fp);
                            remove(temp_path);
                            return HUFF_ERR_IO;
                        }
                        out_pos = 0;
                    }
                    curr = root;
                }
            }
        }

        if (out_pos > 0) {
            crc = huff_crc32_update(crc, out_buf, out_pos);
            if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) {
                huffman_free_tree(root);
                fclose(in_fp);
                fclose(out_fp);
                remove(temp_path);
                return HUFF_ERR_IO;
            }
            out_pos = 0;
        }

        computed_crc = huff_crc32_finalize(crc);
        huffman_free_tree(root);

        if (computed_crc != header.crc32) {
            fclose(in_fp);
            fclose(out_fp);
            remove(temp_path);
            return HUFF_ERR_CRC_MISMATCH;
        }
    }

    fclose(in_fp);

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

    uint64_t in_size = 0;
    huff_get_file_size(in_path, &in_size);

    if (stats) {
        stats->original_size = header.original_size;
        stats->compressed_size = in_size;
        stats->expected_crc32 = header.crc32;
        stats->actual_crc32 = computed_crc;
        stats->elapsed_seconds = elapsed;
    }

    return HUFF_OK;
}

void huff_print_decompress_stats(const HuffDecompressStats* stats) {
    if (!stats) return;

    printf("\n--- Huffman Decompression Statistics ---\n");
    printf("Compressed size:   %llu bytes\n", (unsigned long long)stats->compressed_size);
    printf("Restored size:     %llu bytes\n", (unsigned long long)stats->original_size);
    printf("Expected CRC-32:   0x%08X\n", stats->expected_crc32);
    printf("Calculated CRC-32: 0x%08X (MATCH)\n", stats->actual_crc32);
    if (stats->elapsed_seconds >= 0.0) {
        printf("Elapsed time:      %.4f seconds\n", stats->elapsed_seconds);
    }
    printf("Integrity status:  VERIFIED (100%% byte-for-byte lossless)\n");
    printf("-----------------------------------------\n\n");
}
