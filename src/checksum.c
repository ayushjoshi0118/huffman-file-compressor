#include "checksum.h"

static uint32_t s_crc32_table[256];
static int s_table_initialized = 0;

static void init_crc32_table(void) {
    if (s_table_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) {
                c = 0xEDB88320U ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        s_crc32_table[i] = c;
    }
    s_table_initialized = 1;
}

uint32_t huff_crc32_init(void) {
    init_crc32_table();
    return 0xFFFFFFFFU;
}

uint32_t huff_crc32_update(uint32_t crc, const void* data, size_t length) {
    init_crc32_table();
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < length; i++) {
        crc = s_crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

uint32_t huff_crc32_finalize(uint32_t crc) {
    return crc ^ 0xFFFFFFFFU;
}

uint32_t huff_crc32_compute(const void* data, size_t length) {
    uint32_t crc = huff_crc32_init();
    crc = huff_crc32_update(crc, data, length);
    return huff_crc32_finalize(crc);
}
