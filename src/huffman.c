#include "huffman.h"
#include "checksum.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

/* Frequency analysis pass */
int huff_count_frequencies(FILE* fp, uint64_t* freqs, uint64_t* total_bytes, uint32_t* crc32_out) {
    if (!fp || !freqs || !total_bytes) return HUFF_ERR_ARGS;

    memset(freqs, 0, sizeof(uint64_t) * HUFF_ALPHABET_SIZE);
    *total_bytes = 0;
    uint32_t crc = huff_crc32_init();

    uint8_t buffer[HUFF_BUFFER_SIZE];
    size_t bytes_read = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            freqs[buffer[i]]++;
        }
        crc = huff_crc32_update(crc, buffer, bytes_read);
        *total_bytes += bytes_read;
    }

    if (ferror(fp)) {
        return HUFF_ERR_IO;
    }

    if (crc32_out) {
        *crc32_out = huff_crc32_finalize(crc);
    }

    return HUFF_OK;
}

/* Node creation and cleanup */
HuffmanNode* huffman_create_leaf(uint8_t symbol, uint64_t freq) {
    HuffmanNode* node = (HuffmanNode*)huff_calloc(1, sizeof(HuffmanNode));
    if (!node) return NULL;
    node->freq = freq;
    node->symbol = symbol;
    node->is_leaf = true;
    node->min_symbol = (uint16_t)symbol;
    node->seq_id = 0;
    node->left = NULL;
    node->right = NULL;
    return node;
}

HuffmanNode* huffman_create_internal(HuffmanNode* left, HuffmanNode* right, uint32_t seq_id) {
    HuffmanNode* node = (HuffmanNode*)huff_calloc(1, sizeof(HuffmanNode));
    if (!node) return NULL;
    node->left = left;
    node->right = right;
    node->freq = (left ? left->freq : 0) + (right ? right->freq : 0);
    node->is_leaf = false;
    node->seq_id = seq_id;

    uint16_t min_left = left ? left->min_symbol : 0xFFFF;
    uint16_t min_right = right ? right->min_symbol : 0xFFFF;
    node->min_symbol = (min_left < min_right) ? min_left : min_right;

    return node;
}

void huffman_free_tree(HuffmanNode* root) {
    if (!root) return;
    huffman_free_tree(root->left);
    huffman_free_tree(root->right);
    huff_free(root);
}

/* Min-Heap Priority Queue */
static int compare_nodes(const HuffmanNode* a, const HuffmanNode* b) {
    if (a->freq != b->freq) {
        return (a->freq < b->freq) ? -1 : 1;
    }
    if (a->min_symbol != b->min_symbol) {
        return (a->min_symbol < b->min_symbol) ? -1 : 1;
    }
    if (a->is_leaf != b->is_leaf) {
        return a->is_leaf ? -1 : 1; /* Leaves have priority over internal nodes */
    }
    if (a->seq_id != b->seq_id) {
        return (a->seq_id < b->seq_id) ? -1 : 1;
    }
    return 0;
}

void minheap_init(MinHeap* heap) {
    if (!heap) return;
    heap->size = 0;
    memset(heap->nodes, 0, sizeof(heap->nodes));
}

void minheap_push(MinHeap* heap, HuffmanNode* node) {
    if (!heap || !node || heap->size >= HUFF_ALPHABET_SIZE) return;
    int i = heap->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (compare_nodes(node, heap->nodes[parent]) >= 0) {
            break;
        }
        heap->nodes[i] = heap->nodes[parent];
        i = parent;
    }
    heap->nodes[i] = node;
}

HuffmanNode* minheap_pop(MinHeap* heap) {
    if (!heap || heap->size == 0) return NULL;
    HuffmanNode* min_node = heap->nodes[0];
    heap->size--;
    if (heap->size > 0) {
        HuffmanNode* last = heap->nodes[heap->size];
        int i = 0;
        while (i * 2 + 1 < heap->size) {
            int left = i * 2 + 1;
            int right = left + 1;
            int smallest = left;
            if (right < heap->size && compare_nodes(heap->nodes[right], heap->nodes[left]) < 0) {
                smallest = right;
            }
            if (compare_nodes(last, heap->nodes[smallest]) <= 0) {
                break;
            }
            heap->nodes[i] = heap->nodes[smallest];
            i = smallest;
        }
        heap->nodes[i] = last;
    }
    return min_node;
}

/* Tree construction */
HuffmanNode* huffman_build_tree(const uint64_t* freqs, int* distinct_count) {
    if (!freqs) return NULL;

    MinHeap heap;
    minheap_init(&heap);
    int count = 0;

    for (int i = 0; i < HUFF_ALPHABET_SIZE; i++) {
        if (freqs[i] > 0) {
            HuffmanNode* leaf = huffman_create_leaf((uint8_t)i, freqs[i]);
            if (!leaf) {
                /* Allocation error: cleanup existing nodes */
                while (heap.size > 0) {
                    huffman_free_tree(minheap_pop(&heap));
                }
                return NULL;
            }
            minheap_push(&heap, leaf);
            count++;
        }
    }

    if (distinct_count) {
        *distinct_count = count;
    }

    if (count == 0) {
        return NULL; /* Empty input */
    }

    if (count == 1) {
        return minheap_pop(&heap); /* Single unique symbol */
    }

    uint32_t seq = 0;
    while (heap.size > 1) {
        HuffmanNode* left = minheap_pop(&heap);
        HuffmanNode* right = minheap_pop(&heap);
        HuffmanNode* parent = huffman_create_internal(left, right, ++seq);
        if (!parent) {
            huffman_free_tree(left);
            huffman_free_tree(right);
            while (heap.size > 0) {
                huffman_free_tree(minheap_pop(&heap));
            }
            return NULL;
        }
        minheap_push(&heap, parent);
    }

    return minheap_pop(&heap);
}

/* Code generation */
static void generate_codes_recursive(const HuffmanNode* node,
                                     uint8_t* current_bits,
                                     uint16_t depth,
                                     HuffmanCodeTable* table) {
    if (!node) return;

    if (node->is_leaf) {
        HuffmanCode* code = &table->codes[node->symbol];
        code->length = depth;
        memcpy(code->bits, current_bits, (depth + 7) / 8);
        table->present[node->symbol] = true;
        return;
    }

    if (depth >= HUFF_MAX_CODE_LEN) {
        return; /* Exceeded maximum supported depth */
    }

    /* Left child: bit 0 */
    uint16_t byte_idx = depth / 8;
    uint8_t bit_mask = (uint8_t)(1 << (7 - (depth % 8)));
    current_bits[byte_idx] &= (uint8_t)(~bit_mask); /* clear bit for 0 */
    generate_codes_recursive(node->left, current_bits, depth + 1, table);

    /* Right child: bit 1 */
    current_bits[byte_idx] |= bit_mask; /* set bit for 1 */
    generate_codes_recursive(node->right, current_bits, depth + 1, table);
}

int huffman_generate_codes(const HuffmanNode* root, HuffmanCodeTable* table) {
    if (!root || !table) return HUFF_ERR_ARGS;

    memset(table, 0, sizeof(HuffmanCodeTable));

    if (root->is_leaf) {
        /* Single symbol special case: 1 bit code '0' */
        table->codes[root->symbol].length = 1;
        table->codes[root->symbol].bits[0] = 0;
        table->present[root->symbol] = true;
        return HUFF_OK;
    }

    uint8_t current_bits[(HUFF_MAX_CODE_LEN + 7) / 8];
    memset(current_bits, 0, sizeof(current_bits));
    generate_codes_recursive(root, current_bits, 0, table);

    return HUFF_OK;
}

/* Tree serialization */
int huffman_serialize_tree(const HuffmanNode* root, BitWriter* bw) {
    if (!root || !bw) return HUFF_ERR_ARGS;

    if (root->is_leaf) {
        int res = bitwriter_write_bit(bw, 1);
        if (res != HUFF_OK) return res;
        return bitwriter_write_bits(bw, root->symbol, 8);
    } else {
        int res = bitwriter_write_bit(bw, 0);
        if (res != HUFF_OK) return res;
        res = huffman_serialize_tree(root->left, bw);
        if (res != HUFF_OK) return res;
        return huffman_serialize_tree(root->right, bw);
    }
}

/* Tree deserialization */
static HuffmanNode* deserialize_recursive(BitReader* br,
                                         int* leaf_count,
                                         bool seen_symbols[HUFF_ALPHABET_SIZE],
                                         int depth,
                                         int* error_code) {
    if (depth > HUFF_MAX_CODE_LEN) {
        *error_code = HUFF_ERR_CORRUPT_TREE;
        return NULL;
    }

    int bit = bitreader_read_bit(br);
    if (bit < 0) {
        *error_code = HUFF_ERR_PREMATURE_EOF;
        return NULL;
    }

    if (bit == 1) {
        /* Leaf node */
        int64_t sym = bitreader_read_bits(br, 8);
        if (sym < 0) {
            *error_code = HUFF_ERR_PREMATURE_EOF;
            return NULL;
        }

        uint8_t symbol = (uint8_t)sym;
        if (seen_symbols[symbol]) {
            *error_code = HUFF_ERR_CORRUPT_TREE; /* Duplicate leaf symbol */
            return NULL;
        }

        seen_symbols[symbol] = true;
        (*leaf_count)++;
        if (*leaf_count > HUFF_ALPHABET_SIZE) {
            *error_code = HUFF_ERR_CORRUPT_TREE;
            return NULL;
        }

        HuffmanNode* leaf = huffman_create_leaf(symbol, 0);
        if (!leaf) {
            *error_code = HUFF_ERR_MEMORY;
            return NULL;
        }
        return leaf;
    } else {
        /* Internal node */
        HuffmanNode* left = deserialize_recursive(br, leaf_count, seen_symbols, depth + 1, error_code);
        if (!left) return NULL;

        HuffmanNode* right = deserialize_recursive(br, leaf_count, seen_symbols, depth + 1, error_code);
        if (!right) {
            huffman_free_tree(left);
            return NULL;
        }

        HuffmanNode* internal = huffman_create_internal(left, right, 0);
        if (!internal) {
            huffman_free_tree(left);
            huffman_free_tree(right);
            *error_code = HUFF_ERR_MEMORY;
            return NULL;
        }
        return internal;
    }
}

HuffmanNode* huffman_deserialize_tree(BitReader* br, int* error_code) {
    if (!br || !error_code) {
        if (error_code) *error_code = HUFF_ERR_ARGS;
        return NULL;
    }

    *error_code = HUFF_OK;
    int leaf_count = 0;
    bool seen_symbols[HUFF_ALPHABET_SIZE];
    memset(seen_symbols, 0, sizeof(seen_symbols));

    HuffmanNode* root = deserialize_recursive(br, &leaf_count, seen_symbols, 0, error_code);
    if (!root && *error_code == HUFF_OK) {
        *error_code = HUFF_ERR_CORRUPT_TREE;
    }
    return root;
}
