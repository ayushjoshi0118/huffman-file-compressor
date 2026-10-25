#ifndef HUFF_UTILS_H
#define HUFF_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes returned by library functions and CLI */
enum {
    HUFF_OK = 0,
    HUFF_ERR_ARGS = 1,
    HUFF_ERR_IO = 2,
    HUFF_ERR_FILE_NOT_FOUND = 3,
    HUFF_ERR_OUTPUT_EXISTS = 4,
    HUFF_ERR_SAME_FILE = 5,
    HUFF_ERR_INVALID_MAGIC = 6,
    HUFF_ERR_UNSUPPORTED_VERSION = 7,
    HUFF_ERR_CORRUPT_HEADER = 8,
    HUFF_ERR_CORRUPT_TREE = 9,
    HUFF_ERR_CORRUPT_PAYLOAD = 10,
    HUFF_ERR_PREMATURE_EOF = 11,
    HUFF_ERR_CRC_MISMATCH = 12,
    HUFF_ERR_MEMORY = 13,
    HUFF_ERR_UNKNOWN = 14
};

/* Return human-readable error description */
const char* huff_error_string(int err_code);

/* Endian-safe serialization (Big-Endian / Network Byte Order) */
uint16_t huff_read_u16_be(const uint8_t* p);
void     huff_write_u16_be(uint8_t* p, uint16_t val);
uint32_t huff_read_u32_be(const uint8_t* p);
void     huff_write_u32_be(uint8_t* p, uint32_t val);
uint64_t huff_read_u64_be(const uint8_t* p);
void     huff_write_u64_be(uint8_t* p, uint64_t val);

/* File system operations */
bool huff_file_exists(const char* path);
bool huff_is_same_file(const char* path1, const char* path2);
int  huff_get_file_size(const char* path, uint64_t* size_out);
bool huff_create_temp_path(const char* base_path, char* temp_path_out, size_t max_len);

/* Memory management with optional leak tracking */
void*  huff_malloc(size_t size);
void*  huff_calloc(size_t num, size_t size);
void   huff_free(void* ptr);
size_t huff_get_active_allocations(void);
void   huff_reset_memory_tracker(void);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_UTILS_H */
