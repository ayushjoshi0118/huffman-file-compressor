#ifndef HUFF_HUFFMAN_H
#define HUFF_HUFFMAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bitstream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HUFF_ALPHABET_SIZE 256
#define HUFF_MAX_CODE_LEN  256

/* Huffman Tree Node */
typedef struct HuffmanNode {
    uint64_t freq;
    uint8_t  symbol;
    bool     is_leaf;
    uint16_t min_symbol;  /* Used for deterministic min-heap tie-breaking */
    uint32_t seq_id;      /* Creation order tie-breaking for internal nodes */
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

/* Code representation supporting up to 256-bit codes */
typedef struct {
    uint8_t  bits[(HUFF_MAX_CODE_LEN + 7) / 8];
    uint16_t length; /* Number of valid bits */
} HuffmanCode;

/* Lookup table for all 256 symbols */
typedef struct {
    HuffmanCode codes[HUFF_ALPHABET_SIZE];
    bool        present[HUFF_ALPHABET_SIZE];
} HuffmanCodeTable;

/* Frequency analysis */
int huff_count_frequencies(FILE* fp, uint64_t* freqs, uint64_t* total_bytes, uint32_t* crc32_out);

/* Min-Heap Priority Queue */
typedef struct {
    HuffmanNode* nodes[HUFF_ALPHABET_SIZE];
    int size;
} MinHeap;

void         minheap_init(MinHeap* heap);
void         minheap_push(MinHeap* heap, HuffmanNode* node);
HuffmanNode* minheap_pop(MinHeap* heap);

/* Huffman Tree Operations */
HuffmanNode* huffman_create_leaf(uint8_t symbol, uint64_t freq);
HuffmanNode* huffman_create_internal(HuffmanNode* left, HuffmanNode* right, uint32_t seq_id);
void         huffman_free_tree(HuffmanNode* root);
HuffmanNode* huffman_build_tree(const uint64_t* freqs, int* distinct_count);

/* Code Generation */
int huffman_generate_codes(const HuffmanNode* root, HuffmanCodeTable* table);

/* Tree Serialization & Deserialization */
int          huffman_serialize_tree(const HuffmanNode* root, BitWriter* bw);
HuffmanNode* huffman_deserialize_tree(BitReader* br, int* error_code);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_HUFFMAN_H */
