#include "bitstream.h"
#include "utils.h"
#include <string.h>

int bitwriter_init(BitWriter* bw, FILE* fp) {
    if (!bw || !fp) return HUFF_ERR_ARGS;
    bw->fp = fp;
    bw->buf_pos = 0;
    bw->current_byte = 0;
    bw->bit_pos = 0;
    bw->total_bits_written = 0;
    bw->error = 0;
    return HUFF_OK;
}

int bitwriter_write_bit(BitWriter* bw, int bit) {
    if (!bw || !bw->fp) return HUFF_ERR_ARGS;
    if (bw->error) return HUFF_ERR_IO;

    bw->current_byte = (uint8_t)((bw->current_byte << 1) | (bit & 1));
    bw->bit_pos++;
    bw->total_bits_written++;

    if (bw->bit_pos == 8) {
        bw->buffer[bw->buf_pos++] = bw->current_byte;
        bw->current_byte = 0;
        bw->bit_pos = 0;

        if (bw->buf_pos == HUFF_BUFFER_SIZE) {
            size_t written = fwrite(bw->buffer, 1, bw->buf_pos, bw->fp);
            if (written != bw->buf_pos) {
                bw->error = 1;
                return HUFF_ERR_IO;
            }
            bw->buf_pos = 0;
        }
    }
    return HUFF_OK;
}

int bitwriter_write_bits(BitWriter* bw, uint32_t val, int count) {
    if (!bw || count < 0 || count > 32) return HUFF_ERR_ARGS;
    for (int i = count - 1; i >= 0; i--) {
        int bit = (int)((val >> i) & 1);
        int res = bitwriter_write_bit(bw, bit);
        if (res != HUFF_OK) return res;
    }
    return HUFF_OK;
}

int bitwriter_write_code(BitWriter* bw, const uint8_t* bits, uint16_t length) {
    if (!bw || !bits) return HUFF_ERR_ARGS;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte_val = bits[i / 8];
        int bit = (byte_val >> (7 - (i % 8))) & 1;
        int res = bitwriter_write_bit(bw, bit);
        if (res != HUFF_OK) return res;
    }
    return HUFF_OK;
}

int bitwriter_flush(BitWriter* bw) {
    if (!bw || !bw->fp) return HUFF_ERR_ARGS;
    if (bw->error) return HUFF_ERR_IO;

    if (bw->bit_pos > 0) {
        /* Pad remaining bits in the byte with zeros */
        bw->current_byte = (uint8_t)(bw->current_byte << (8 - bw->bit_pos));
        if (bw->buf_pos >= HUFF_BUFFER_SIZE) {
            size_t written = fwrite(bw->buffer, 1, bw->buf_pos, bw->fp);
            if (written != bw->buf_pos) {
                bw->error = 1;
                return HUFF_ERR_IO;
            }
            bw->buf_pos = 0;
        }
        bw->buffer[bw->buf_pos++] = bw->current_byte;
        bw->current_byte = 0;
        bw->bit_pos = 0;
    }

    if (bw->buf_pos > 0) {
        size_t written = fwrite(bw->buffer, 1, bw->buf_pos, bw->fp);
        if (written != bw->buf_pos) {
            bw->error = 1;
            return HUFF_ERR_IO;
        }
        bw->buf_pos = 0;
    }

    if (fflush(bw->fp) != 0) {
        bw->error = 1;
        return HUFF_ERR_IO;
    }

    return HUFF_OK;
}

uint64_t bitwriter_total_bytes_written(const BitWriter* bw) {
    if (!bw) return 0;
    return (bw->total_bits_written + 7) / 8;
}

int bitreader_init(BitReader* br, FILE* fp) {
    if (!br || !fp) return HUFF_ERR_ARGS;
    br->fp = fp;
    br->buf_size = 0;
    br->buf_pos = 0;
    br->current_byte = 0;
    br->bit_pos = -1;
    br->total_bits_read = 0;
    br->eof_reached = false;
    br->error = 0;
    return HUFF_OK;
}

int bitreader_read_bit(BitReader* br) {
    if (!br || !br->fp) return -1;
    if (br->error || br->eof_reached) return -1;

    if (br->bit_pos < 0) {
        if (br->buf_pos >= br->buf_size) {
            br->buf_size = fread(br->buffer, 1, HUFF_BUFFER_SIZE, br->fp);
            br->buf_pos = 0;
            if (br->buf_size == 0) {
                if (ferror(br->fp)) {
                    br->error = 1;
                }
                br->eof_reached = true;
                return -1;
            }
        }
        br->current_byte = br->buffer[br->buf_pos++];
        br->bit_pos = 7;
    }

    int bit = (br->current_byte >> br->bit_pos) & 1;
    br->bit_pos--;
    br->total_bits_read++;
    return bit;
}

int64_t bitreader_read_bits(BitReader* br, int count) {
    if (!br || count < 0 || count > 32) return -1;
    uint32_t val = 0;
    for (int i = 0; i < count; i++) {
        int b = bitreader_read_bit(br);
        if (b < 0) return -1;
        val = (val << 1) | (uint32_t)b;
    }
    return (int64_t)val;
}

uint64_t bitreader_total_bits_read(const BitReader* br) {
    if (!br) return 0;
    return br->total_bits_read;
}
