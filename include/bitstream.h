#ifndef HUFF_BITSTREAM_H
#define HUFF_BITSTREAM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUFF_BUFFER_SIZE 65536

/* BitWriter: Writes bits MSB-first into a buffered binary stream */
typedef struct {
    FILE* fp;
    uint8_t buffer[HUFF_BUFFER_SIZE];
    size_t buf_pos;
    uint8_t current_byte;
    uint8_t bit_pos;             /* Number of bits accumulated in current_byte (0-7) */
    uint64_t total_bits_written; /* Total count of payload bits written */
    int error;                   /* Non-zero if an I/O error occurred */
} BitWriter;

/* BitReader: Reads bits MSB-first from a buffered binary stream */
typedef struct {
    FILE* fp;
    uint8_t buffer[HUFF_BUFFER_SIZE];
    size_t buf_size;
    size_t buf_pos;
    uint8_t current_byte;
    int8_t bit_pos;            /* Current bit index within current_byte (7 down to 0, or -1 when empty) */
    uint64_t total_bits_read;  /* Total count of bits read */
    bool eof_reached;          /* True when underlying stream reached EOF */
    int error;                 /* Non-zero if an I/O error occurred */
} BitReader;

/* BitWriter functions */
int  bitwriter_init(BitWriter* bw, FILE* fp);
int  bitwriter_write_bit(BitWriter* bw, int bit);
int  bitwriter_write_bits(BitWriter* bw, uint32_t val, int count);
int  bitwriter_write_code(BitWriter* bw, const uint8_t* bits, uint16_t length);
int  bitwriter_flush(BitWriter* bw);
uint64_t bitwriter_total_bytes_written(const BitWriter* bw);

/* BitReader functions */
int     bitreader_init(BitReader* br, FILE* fp);
int     bitreader_read_bit(BitReader* br);
int64_t bitreader_read_bits(BitReader* br, int count);
uint64_t bitreader_total_bits_read(const BitReader* br);

#ifdef __cplusplus
}
#endif

#endif /* HUFF_BITSTREAM_H */
