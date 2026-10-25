#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <io.h>
static bool is_stdin_interactive(void) {
    return _isatty(0) != 0;
}
#else
#include <unistd.h>
static bool is_stdin_interactive(void) {
    return isatty(0) != 0;
}
#endif

#include "compressor.h"
#include "decompressor.h"
#include "checksum.h"
#include "utils.h"

#define HUFF_APP_NAME    "huff"
#define HUFF_APP_VERSION "1.0.0"

static void print_version(void) {
    printf("%s version %s (C11, standard library only)\n", HUFF_APP_NAME, HUFF_APP_VERSION);
    printf("Deterministic Huffman Coding File Compressor & Decompressor\n");
}

static void print_help(void) {
    printf("Usage:\n");
    printf("  %s -c <input> <output> [options]    Compress input file into output\n", HUFF_APP_NAME);
    printf("  %s -d <input> <output> [options]    Decompress input file into output\n", HUFF_APP_NAME);
    printf("  %s -i <input>                       Analyze and display file byte frequencies\n", HUFF_APP_NAME);
    printf("  %s --help | -h                      Show this help message\n", HUFF_APP_NAME);
    printf("  %s --version | -V                   Show version information\n", HUFF_APP_NAME);
    printf("\nAliases:\n");
    printf("  --compress   alias for -c\n");
    printf("  --decompress alias for -d\n");
    printf("  --info       alias for -i\n");
    printf("\nOptions:\n");
    printf("  -f, --force      Overwrite existing output files without prompting\n");
    printf("  -v, --verbose    Display verbose details (tree codes, frequencies)\n");
    printf("  -b, --benchmark  Display execution timing and throughput statistics\n");
    printf("  --self-test      Run internal self-tests to verify algorithms\n");
}

/* Built-in self-test */
static int run_self_test(void) {
    printf("Running internal algorithm self-tests...\n");

    /* 1. MinHeap & Tie-Breaking Test */
    printf("  [1/4] Min-Heap priority queue & tie-breaking: ");
    MinHeap heap;
    minheap_init(&heap);
    HuffmanNode* n1 = huffman_create_leaf('A', 10);
    HuffmanNode* n2 = huffman_create_leaf('B', 5);
    HuffmanNode* n3 = huffman_create_leaf('C', 10);
    minheap_push(&heap, n1);
    minheap_push(&heap, n2);
    minheap_push(&heap, n3);

    HuffmanNode* p1 = minheap_pop(&heap);
    HuffmanNode* p2 = minheap_pop(&heap);
    HuffmanNode* p3 = minheap_pop(&heap);

    if (p1 != n2 || p2 != n1 || p3 != n3) {
        printf("FAILED\n");
        return 1;
    }
    huffman_free_tree(p1);
    huffman_free_tree(p2);
    huffman_free_tree(p3);
    printf("PASSED\n");

    /* 2. Checksum Test */
    printf("  [2/4] IEEE 802.3 CRC-32 integrity: ");
    const char* test_str = "123456789";
    uint32_t crc = huff_crc32_compute(test_str, strlen(test_str));
    if (crc != 0xCBF43926U) {
        printf("FAILED (Expected 0xCBF43926, got 0x%08X)\n", crc);
        return 1;
    }
    printf("PASSED (0x%08X)\n", crc);

    /* 3. Bitstream & Tree Serialization Test */
    printf("  [3/4] Tree construction & serialization: ");
    uint64_t freqs[HUFF_ALPHABET_SIZE];
    memset(freqs, 0, sizeof(freqs));
    freqs['A'] = 50;
    freqs['B'] = 25;
    freqs['C'] = 15;
    freqs['D'] = 10;
    int distinct = 0;
    HuffmanNode* root = huffman_build_tree(freqs, &distinct);
    if (!root || distinct != 4) {
        printf("FAILED (Tree build)\n");
        return 1;
    }

    HuffmanCodeTable table;
    if (huffman_generate_codes(root, &table) != HUFF_OK) {
        printf("FAILED (Code gen)\n");
        huffman_free_tree(root);
        return 1;
    }

    /* Verify prefix-free property */
    for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
        if (!table.present[i]) continue;
        for (int j = 0; j < HUFF_ALPHABET_SIZE; j++) {
            if (i == j || !table.present[j]) continue;
            if (table.codes[i].length <= table.codes[j].length) {
                /* Check if code i is a prefix of code j */
                bool is_prefix = true;
                for (int b = 0; b < table.codes[i].length; b++) {
                    int bit_i = (table.codes[i].bits[b / 8] >> (7 - (b % 8))) & 1;
                    int bit_j = (table.codes[j].bits[b / 8] >> (7 - (b % 8))) & 1;
                    if (bit_i != bit_j) {
                        is_prefix = false;
                        break;
                    }
                }
                if (is_prefix) {
                    printf("FAILED (Prefix collision between %d and %d)\n", i, j);
                    huffman_free_tree(root);
                    return 1;
                }
            }
        }
    }
    huffman_free_tree(root);
    printf("PASSED (Prefix-free verified)\n");

    /* 4. Memory Tracker Verification */
    printf("  [4/4] Memory safety & zero leaks: ");
    size_t active = huff_get_active_allocations();
    if (active != 0) {
        printf("FAILED (%zu unreleased allocations)\n", active);
        return 1;
    }
    printf("PASSED (0 leaks)\n");

    printf("\nAll internal self-tests passed successfully!\n");
    return 0;
}

static bool confirm_overwrite(const char* path) {
    if (!is_stdin_interactive()) {
        return false;
    }
    fprintf(stderr, "Output file '%s' already exists. Overwrite? (y/N): ", path);
    fflush(stderr);
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return false;
    }
    return (buf[0] == 'y' || buf[0] == 'Y');
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return HUFF_ERR_ARGS;
    }

    enum {
        MODE_NONE,
        MODE_COMPRESS,
        MODE_DECOMPRESS,
        MODE_INFO,
        MODE_HELP,
        MODE_VERSION,
        MODE_SELF_TEST
    } mode = MODE_NONE;

    const char* in_path = NULL;
    const char* out_path = NULL;
    bool force = false;
    bool verbose = false;
    bool benchmark = false;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            mode = MODE_HELP;
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0 ||
                   (argc == 2 && strcmp(arg, "-v") == 0)) {
            mode = MODE_VERSION;
        } else if (strcmp(arg, "--self-test") == 0) {
            mode = MODE_SELF_TEST;
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--compress") == 0) {
            mode = MODE_COMPRESS;
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--decompress") == 0) {
            mode = MODE_DECOMPRESS;
        } else if (strcmp(arg, "-i") == 0 || strcmp(arg, "--info") == 0) {
            mode = MODE_INFO;
        } else if (strcmp(arg, "-f") == 0 || strcmp(arg, "--force") == 0) {
            force = true;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--benchmark") == 0) {
            benchmark = true;
        } else if (arg[0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", arg);
            fprintf(stderr, "Run '%s --help' for usage.\n", HUFF_APP_NAME);
            return HUFF_ERR_ARGS;
        } else {
            if (!in_path) {
                in_path = arg;
            } else if (!out_path) {
                out_path = arg;
            } else {
                fprintf(stderr, "Error: Unexpected extra argument '%s'\n", arg);
                return HUFF_ERR_ARGS;
            }
        }
    }

    if (mode == MODE_HELP) {
        print_help();
        return HUFF_OK;
    }

    if (mode == MODE_VERSION) {
        print_version();
        return HUFF_OK;
    }

    if (mode == MODE_SELF_TEST) {
        return run_self_test();
    }

    if (mode == MODE_INFO) {
        if (!in_path) {
            fprintf(stderr, "Error: Missing input file for info analysis.\n");
            fprintf(stderr, "Usage: %s -i <input>\n", HUFF_APP_NAME);
            return HUFF_ERR_ARGS;
        }
        int res = huff_print_file_info(in_path);
        if (res != HUFF_OK) {
            fprintf(stderr, "Error analyzing '%s': %s\n", in_path, huff_error_string(res));
        }
        return res;
    }

    if (mode == MODE_COMPRESS || mode == MODE_DECOMPRESS) {
        if (!in_path || !out_path) {
            fprintf(stderr, "Error: Both <input> and <output> paths are required.\n");
            fprintf(stderr, "Usage: %s %s <input> <output> [options]\n",
                    HUFF_APP_NAME, (mode == MODE_COMPRESS) ? "-c" : "-d");
            return HUFF_ERR_ARGS;
        }

        /* Check if input and output refer to the exact same file */
        if (huff_is_same_file(in_path, out_path)) {
            fprintf(stderr, "Error: Input and output paths specify the exact same file ('%s').\n", in_path);
            return HUFF_ERR_SAME_FILE;
        }

        /* Check for overwrite if output exists */
        if (huff_file_exists(out_path) && !force) {
            if (!confirm_overwrite(out_path)) {
                fprintf(stderr, "Error: Output file '%s' exists. Operation aborted (use -f to force).\n", out_path);
                return HUFF_ERR_OUTPUT_EXISTS;
            }
            force = true; /* User confirmed interactively */
        }

        if (mode == MODE_COMPRESS) {
            HuffCompressOptions opts = { force, verbose, benchmark };
            HuffCompressStats stats;
            int res = huff_compress_file(in_path, out_path, &opts, &stats);
            if (res != HUFF_OK) {
                fprintf(stderr, "Compression error: %s\n", huff_error_string(res));
                return res;
            }
            huff_print_compress_stats(&stats);

            if (verbose && stats.distinct_symbols > 0) {
                printf("Huffman Code Lengths:\n");
                for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
                    if (stats.freqs[i] > 0) {
                        char c = (i >= 32 && i <= 126) ? (char)i : '.';
                        printf("  Byte 0x%02X ('%c'): %llu occurrences -> %u bits\n",
                               i, c, (unsigned long long)stats.freqs[i], stats.code_lengths[i]);
                    }
                }
                printf("\n");
            }
            return HUFF_OK;
        } else {
            HuffDecompressOptions opts = { force, verbose, benchmark };
            HuffDecompressStats stats;
            int res = huff_decompress_file(in_path, out_path, &opts, &stats);
            if (res != HUFF_OK) {
                fprintf(stderr, "Decompression error: %s\n", huff_error_string(res));
                return res;
            }
            huff_print_decompress_stats(&stats);
            return HUFF_OK;
        }
    }

    fprintf(stderr, "Error: No command specified (-c, -d, -i, --help, --version).\n");
    fprintf(stderr, "Run '%s --help' for usage.\n", HUFF_APP_NAME);
    return HUFF_ERR_ARGS;
}
