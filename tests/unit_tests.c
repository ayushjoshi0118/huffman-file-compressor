#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "checksum.h"
#include "bitstream.h"
#include "huffman.h"
#include "file_format.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    printf("Running %s... ", #fn); \
    if (fn()) { \
        g_tests_passed++; \
        printf("PASSED\n"); \
    } else { \
        printf("FAILED\n"); \
    } \
} while (0)

/* 1. Test Endian Serialization */
static int test_endian_helpers(void) {
    uint8_t buf[8];

    huff_write_u16_be(buf, 0x1234);
    TEST_ASSERT(buf[0] == 0x12 && buf[1] == 0x34, "u16 write");
    TEST_ASSERT(huff_read_u16_be(buf) == 0x1234, "u16 read");

    huff_write_u32_be(buf, 0xDEADBEEF);
    TEST_ASSERT(buf[0] == 0xDE && buf[1] == 0xAD && buf[2] == 0xBE && buf[3] == 0xEF, "u32 write");
    TEST_ASSERT(huff_read_u32_be(buf) == 0xDEADBEEF, "u32 read");

    huff_write_u64_be(buf, 0x0123456789ABCDEFULL);
    TEST_ASSERT(buf[0] == 0x01 && buf[7] == 0xEF, "u64 write");
    TEST_ASSERT(huff_read_u64_be(buf) == 0x0123456789ABCDEFULL, "u64 read");

    return 1;
}

/* 2. Test CRC-32 Checksum */
static int test_checksum_crc32(void) {
    /* Standard test vector: "123456789" -> 0xCBF43926 */
    const char* str = "123456789";
    uint32_t crc = huff_crc32_compute(str, strlen(str));
    TEST_ASSERT(crc == 0xCBF43926U, "CRC32 standard vector 123456789");

    /* Empty string test vector */
    uint32_t empty_crc = huff_crc32_compute("", 0);
    TEST_ASSERT(empty_crc == 0x00000000U, "CRC32 empty buffer");

    /* Incremental update verification */
    uint32_t inc_crc = huff_crc32_init();
    inc_crc = huff_crc32_update(inc_crc, "1234", 4);
    inc_crc = huff_crc32_update(inc_crc, "56789", 5);
    inc_crc = huff_crc32_finalize(inc_crc);
    TEST_ASSERT(inc_crc == 0xCBF43926U, "CRC32 incremental update");

    return 1;
}

/* 3. Test BitWriter & BitReader Round-Trip */
static int test_bitstream_roundtrip(void) {
    const char* temp_path = "unit_test_bits.tmp";
    FILE* fp = fopen(temp_path, "wb");
    TEST_ASSERT(fp != NULL, "create temp file for bitstream");

    BitWriter bw;
    TEST_ASSERT(bitwriter_init(&bw, fp) == HUFF_OK, "bitwriter init");

    /* Write known bit sequence: 1, 0, 1, 1, 0, 0, 1, 0 (0xB2) + 1, 1 (padded) */
    int bits_to_write[] = { 1, 0, 1, 1, 0, 0, 1, 0, 1, 1 };
    size_t num_bits = sizeof(bits_to_write) / sizeof(bits_to_write[0]);

    for (size_t i = 0; i < num_bits; i++) {
        TEST_ASSERT(bitwriter_write_bit(&bw, bits_to_write[i]) == HUFF_OK, "write bit");
    }

    /* Write a 16-bit word */
    TEST_ASSERT(bitwriter_write_bits(&bw, 0xABCD, 16) == HUFF_OK, "write bits 16");

    TEST_ASSERT(bitwriter_flush(&bw) == HUFF_OK, "flush bitwriter");
    fclose(fp);

    /* Read back using BitReader */
    fp = fopen(temp_path, "rb");
    TEST_ASSERT(fp != NULL, "open temp file for bitstream read");

    BitReader br;
    TEST_ASSERT(bitreader_init(&br, fp) == HUFF_OK, "bitreader init");

    for (size_t i = 0; i < num_bits; i++) {
        int b = bitreader_read_bit(&br);
        TEST_ASSERT(b == bits_to_write[i], "read bit matches written bit");
    }

    int64_t val = bitreader_read_bits(&br, 16);
    TEST_ASSERT(val == 0xABCD, "read 16 bits matches written word");

    fclose(fp);
    remove(temp_path);
    return 1;
}

/* 4. Test 32-Bit Bitstream Read (Ensuring MSB sign safety with int64_t) */
static int test_bitstream_32bit(void) {
    const char* temp_path = "unit_test_32bit.tmp";
    FILE* fp = fopen(temp_path, "wb");
    TEST_ASSERT(fp != NULL, "create temp file for 32bit test");

    BitWriter bw;
    bitwriter_init(&bw, fp);
    /* Write 0xDEADBEEF where bit 31 is 1 */
    TEST_ASSERT(bitwriter_write_bits(&bw, 0xDEADBEEF, 32) == HUFF_OK, "write 32 bits");
    TEST_ASSERT(bitwriter_flush(&bw) == HUFF_OK, "flush bitwriter");
    fclose(fp);

    fp = fopen(temp_path, "rb");
    TEST_ASSERT(fp != NULL, "open temp file for 32bit read");

    BitReader br;
    bitreader_init(&br, fp);
    int64_t val = bitreader_read_bits(&br, 32);
    TEST_ASSERT(val == 0xDEADBEEFULL, "read 32 bits preserves full unsigned value without sign error");

    fclose(fp);
    remove(temp_path);
    return 1;
}

/* 4. Test Min-Heap & Deterministic Tie-Breaking */
static int test_minheap_and_tie_breaking(void) {
    MinHeap heap;
    minheap_init(&heap);

    /* Insert nodes with equal frequencies but different symbols */
    HuffmanNode* n_z = huffman_create_leaf('Z', 10);
    HuffmanNode* n_a = huffman_create_leaf('A', 10);
    HuffmanNode* n_m = huffman_create_leaf('M', 5);

    minheap_push(&heap, n_z);
    minheap_push(&heap, n_a);
    minheap_push(&heap, n_m);

    /* M has lowest frequency 5 */
    HuffmanNode* p1 = minheap_pop(&heap);
    TEST_ASSERT(p1 == n_m, "Lowest frequency node popped first");

    /* A and Z have equal frequency 10, tie-breaker must choose A first */
    HuffmanNode* p2 = minheap_pop(&heap);
    TEST_ASSERT(p2 == n_a, "Tie-breaker selects smaller symbol first");

    HuffmanNode* p3 = minheap_pop(&heap);
    TEST_ASSERT(p3 == n_z, "Remaining node popped");

    huffman_free_tree(p1);
    huffman_free_tree(p2);
    huffman_free_tree(p3);

    return 1;
}

/* 5. Test Full Huffman Tree Construction and Prefix-Free Codes */
static int test_huffman_codes_prefix_free(void) {
    uint64_t freqs[HUFF_ALPHABET_SIZE];
    memset(freqs, 0, sizeof(freqs));

    /* Populate with varying frequencies */
    for (int i = 0; i < 256; i++) {
        freqs[i] = (uint64_t)(i + 1) * 3;
    }

    int distinct = 0;
    HuffmanNode* root = huffman_build_tree(freqs, &distinct);
    TEST_ASSERT(root != NULL, "build tree for all 256 symbols");
    TEST_ASSERT(distinct == 256, "all 256 symbols present");

    HuffmanCodeTable table;
    TEST_ASSERT(huffman_generate_codes(root, &table) == HUFF_OK, "generate codes");

    /* Verify all 256 symbols have codes */
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT(table.present[i] == true, "symbol present in code table");
        TEST_ASSERT(table.codes[i].length > 0, "code length > 0");
    }

    /* Check prefix-free property across all symbol pairs */
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 256; j++) {
            if (i == j) continue;
            if (table.codes[i].length <= table.codes[j].length) {
                bool is_prefix = true;
                for (int b = 0; b < table.codes[i].length; b++) {
                    int bit_i = (table.codes[i].bits[b / 8] >> (7 - (b % 8))) & 1;
                    int bit_j = (table.codes[j].bits[b / 8] >> (7 - (b % 8))) & 1;
                    if (bit_i != bit_j) {
                        is_prefix = false;
                        break;
                    }
                }
                TEST_ASSERT(!is_prefix, "Code must be strictly prefix-free");
            }
        }
    }

    huffman_free_tree(root);
    return 1;
}

/* 6. Test Tree Serialization & Deserialization Round-Trip */
static int test_tree_serialization_roundtrip(void) {
    uint64_t freqs[HUFF_ALPHABET_SIZE];
    memset(freqs, 0, sizeof(freqs));
    freqs['H'] = 10;
    freqs['U'] = 20;
    freqs['F'] = 30;

    int distinct = 0;
    HuffmanNode* orig_root = huffman_build_tree(freqs, &distinct);
    TEST_ASSERT(orig_root != NULL, "build tree");

    const char* temp_path = "unit_test_tree.tmp";
    FILE* fp = fopen(temp_path, "wb");
    TEST_ASSERT(fp != NULL, "create tree temp file");

    BitWriter bw;
    bitwriter_init(&bw, fp);
    TEST_ASSERT(huffman_serialize_tree(orig_root, &bw) == HUFF_OK, "serialize tree");
    bitwriter_flush(&bw);
    fclose(fp);

    /* Read back */
    fp = fopen(temp_path, "rb");
    TEST_ASSERT(fp != NULL, "open tree temp file");

    BitReader br;
    bitreader_init(&br, fp);

    int err = HUFF_OK;
    HuffmanNode* restored_root = huffman_deserialize_tree(&br, &err);
    TEST_ASSERT(restored_root != NULL, "deserialize tree");
    TEST_ASSERT(err == HUFF_OK, "no deserialization error");

    HuffmanCodeTable orig_table, restored_table;
    huffman_generate_codes(orig_root, &orig_table);
    huffman_generate_codes(restored_root, &restored_table);

    /* Both trees must produce identical code lengths */
    for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
        TEST_ASSERT(orig_table.present[i] == restored_table.present[i], "same symbols present");
        if (orig_table.present[i]) {
            TEST_ASSERT(orig_table.codes[i].length == restored_table.codes[i].length, "code lengths match");
        }
    }

    huffman_free_tree(orig_root);
    huffman_free_tree(restored_root);
    fclose(fp);
    remove(temp_path);
    return 1;
}

/* 7. Test Header Validation & Error Detection */
static int test_header_validation(void) {
    HuffHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.magic = HUFF_MAGIC_VAL;
    hdr.version = HUFF_VERSION_1;
    hdr.flags = HUFF_FLAG_HAS_CRC32;
    hdr.original_size = 1000;
    hdr.crc32 = 0x12345678;

    TEST_ASSERT(huff_validate_header(&hdr) == HUFF_OK, "valid header");

    /* Corrupted magic */
    hdr.magic = 0x11223344;
    TEST_ASSERT(huff_validate_header(&hdr) == HUFF_ERR_INVALID_MAGIC, "invalid magic detected");
    hdr.magic = HUFF_MAGIC_VAL;

    /* Unsupported version */
    hdr.version = 99;
    TEST_ASSERT(huff_validate_header(&hdr) == HUFF_ERR_UNSUPPORTED_VERSION, "unsupported version detected");
    hdr.version = HUFF_VERSION_1;

    /* Conflicting empty and single symbol flags */
    hdr.flags = HUFF_FLAG_EMPTY_FILE | HUFF_FLAG_SINGLE_SYMBOL;
    TEST_ASSERT(huff_validate_header(&hdr) == HUFF_ERR_CORRUPT_HEADER, "conflicting flags detected");

    /* Empty size without empty flag */
    hdr.flags = HUFF_FLAG_HAS_CRC32;
    hdr.original_size = 0;
    TEST_ASSERT(huff_validate_header(&hdr) == HUFF_ERR_CORRUPT_HEADER, "zero size without empty flag detected");

    /* Reserved field non-zero */
    hdr.original_size = 100;
    hdr.reserved = 0x1234;
    TEST_ASSERT(huff_validate_header(&hdr) == HUFF_ERR_CORRUPT_HEADER, "non-zero reserved field detected");

    return 1;
}

/* 9. Test Single Symbol Tree Deserialization */
static int test_single_symbol_tree_deserialization(void) {
    HuffmanNode* leaf = huffman_create_leaf('S', 10);
    TEST_ASSERT(leaf != NULL, "create single leaf");

    const char* temp_path = "unit_test_single_tree.tmp";
    FILE* fp = fopen(temp_path, "wb");
    TEST_ASSERT(fp != NULL, "open single tree temp file");

    BitWriter bw;
    bitwriter_init(&bw, fp);
    TEST_ASSERT(huffman_serialize_tree(leaf, &bw) == HUFF_OK, "serialize single leaf");
    bitwriter_flush(&bw);
    fclose(fp);

    fp = fopen(temp_path, "rb");
    TEST_ASSERT(fp != NULL, "open single tree for read");

    BitReader br;
    bitreader_init(&br, fp);
    int err = HUFF_OK;
    HuffmanNode* restored = huffman_deserialize_tree(&br, &err);
    TEST_ASSERT(restored != NULL, "deserialize single leaf");
    TEST_ASSERT(err == HUFF_OK, "no error deserializing single leaf");
    TEST_ASSERT(restored->is_leaf == true, "restored node is leaf");
    TEST_ASSERT(restored->symbol == 'S', "restored symbol is S");

    huffman_free_tree(leaf);
    huffman_free_tree(restored);
    fclose(fp);
    remove(temp_path);
    return 1;
}

/* 10. Memory Leak Verification */
static int test_memory_leak_check(void) {
    size_t active = huff_get_active_allocations();
    TEST_ASSERT(active == 0, "active allocations must be exactly 0 (no leaks)");
    return 1;
}

int main(void) {
    printf("=== Huffman File Compressor C Unit Tests ===\n\n");

    RUN_TEST(test_endian_helpers);
    RUN_TEST(test_checksum_crc32);
    RUN_TEST(test_bitstream_roundtrip);
    RUN_TEST(test_bitstream_32bit);
    RUN_TEST(test_minheap_and_tie_breaking);
    RUN_TEST(test_huffman_codes_prefix_free);
    RUN_TEST(test_tree_serialization_roundtrip);
    RUN_TEST(test_single_symbol_tree_deserialization);
    RUN_TEST(test_header_validation);
    RUN_TEST(test_memory_leak_check);

    printf("\nUnit Test Summary: %d / %d tests passed.\n", g_tests_passed, g_tests_run);

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
