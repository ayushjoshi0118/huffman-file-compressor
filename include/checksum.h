#ifndef HUFF_CHECKSUM_H
#define HUFF_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize CRC-32 computation */
uint32_t huff_crc32_init(void);

/* Update running CRC-32 with a buffer of bytes */
uint32_t huff_crc32_update(uint32_t crc, const void* data, size_t length);

/* Finalize CRC-32 computation */
uint32_t huff_crc32_finalize(uint32_t crc);

/* Calculate CRC-32 over an entire memory buffer in one call */
uint32_t huff_crc32_compute(const void* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_CHECKSUM_H */
