# Huffman File Compressor: Binary Format Specification (HUFFv1)

This document provides the formal specification of the custom `.huff` binary compressed file format implemented by the Huffman File Compressor tool.

---

## 1. Format Design Goals

1. **Portability**: All multi-byte integers are serialized in standard Big-Endian (Network Byte Order). No raw C `struct` dumps are written to disk, avoiding struct padding, alignment, and endianness issues across different architectures.
2. **Safety & Self-Containment**: The file format is self-delimiting and stores original uncompressed size and an IEEE 802.3 CRC-32 checksum. Corrupted or truncated files are rejected before causing invalid memory access.
3. **Efficiency**: Canonical pre-order bitstream serialization of the Huffman tree requires only $\approx 25$ to $320$ bytes of metadata rather than a naive 2048-byte frequency table.
4. **Edge-Case Support**: Dedicated flags handle 0-byte (empty) and 1-symbol repeated files with optimal header-only representations.

---

## 2. Header Layout

The compressed file begins with a fixed-width binary header followed by optional single-symbol data and the variable-length bitstream:

| Offset (Bytes) | Field Name | Width (Bytes) | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x00 - 0x03` | `magic` | 4 | ASCII String | Magic bytes: `'H'`, `'U'`, `'F'`, `'F'` (`0x48554646`) |
| `0x04` | `version` | 1 | `uint8_t` | File format version (`0x01` for HUFFv1) |
| `0x05` | `flags` | 1 | `uint8_t` | Bit flags describing special file conditions |
| `0x06 - 0x07` | `reserved` | 2 | `uint16_t` | Reserved for future format expansions (must be `0x0000`) |
| `0x08 - 0x0F` | `orig_size` | 8 | `uint64_t` | Original uncompressed file size in bytes (Big-Endian) |
| `0x10 - 0x13` | `crc32` | 4 | `uint32_t` | IEEE 802.3 CRC-32 checksum of original file (Big-Endian) |
| `0x14` | `single_sym` | 0 or 1 | `uint8_t` | Single symbol byte (present **only** if `FLAG_SINGLE_SYMBOL` is set) |

### Total Base Header Size
- Standard file ($\ge 2$ symbols): **20 bytes**
- Single-symbol file: **21 bytes**
- Empty file: **20 bytes**

---

## 3. Flag Definitions

The `flags` byte at offset `0x05` defines the mode:

- `0x01` (`HUFF_FLAG_EMPTY_FILE`): The original file is 0 bytes. `orig_size` must be 0. No tree or payload follows.
- `0x02` (`HUFF_FLAG_SINGLE_SYMBOL`): The original file contains only 1 unique byte repeated `orig_size` times. The single symbol byte is stored at offset `0x14`. No tree or bitstream payload follows.
- `0x04` (`HUFF_FLAG_HAS_CRC32`): Indicates that the header contains a valid CRC-32 checksum at offset `0x10`. (Always set in HUFFv1).

`FLAG_EMPTY_FILE` and `FLAG_SINGLE_SYMBOL` are mutually exclusive.

---

## 4. Huffman Tree Serialization

For standard files ($\ge 2$ distinct symbols), the serialized Huffman tree immediately follows the 20-byte header within the bitstream.

The tree is written using a pre-order traversal of the full binary tree:
- **Internal Node**: Emits **1 bit** with value `0`.
  - Followed recursively by the left child bitstream, then right child bitstream.
- **Leaf Node**: Emits **1 bit** with value `1`, followed immediately by **8 bits** representing the symbol byte value (`0x00` - `0xFF`).

### Bitstream Delimitation
Because every internal node in a Huffman tree has exactly two children and leaves have zero children, the pre-order traversal is **strictly self-delimiting**. The decompressor reads until the tree is fully reconstructed, and the very next bit in the bitstream is the start of the compressed payload.

### Size Efficiency
For an alphabet of $N$ distinct symbols ($2 \le N \le 256$):
- Tree contains $N$ leaves and $N - 1$ internal nodes.
- Total bits for tree structure = $(2N - 1) + 8N = 10N - 1$ bits.
- Maximum tree size (all 256 symbols present): $2559$ bits = **320 bytes**.
- For typical text (e.g. 64 distinct symbols): $\approx 639$ bits = **80 bytes**.

---

## 5. Encoded Payload & Bit Order

The payload bits directly follow the serialized tree.

### Bit Ordering
- **Most-Significant-Bit (MSB) First**: Bits are accumulated into bytes from MSB (bit index 7) down to LSB (bit index 0).
- Example: If a Huffman code is `101`, bit `1` is placed in the highest remaining bit of the byte, followed by `0`, followed by `1`.

### Padding & Exact Termination
- When the encoder finishes writing the payload, any remaining unfilled bits in the final byte are padded with `0`s to align to the byte boundary (`bitwriter_flush`).
- **Decompression Termination**: The decompressor decodes symbols until `bytes_decoded == orig_size`. The decoder **never** attempts to decode past `orig_size`. This completely eliminates the ambiguity of trailing padding bits.

---

## 6. Validation & Security Defenses

The decompressor performs strict pre-execution and runtime validation:

1. **Magic Signature Check**: `magic == 0x48554646` (`"HUFF"`). Non-matching files are immediately rejected with error code 6 (`HUFF_ERR_INVALID_MAGIC`).
2. **Version Check**: `version == 1`. Unsupported versions are rejected with error code 7 (`HUFF_ERR_UNSUPPORTED_VERSION`).
3. **Reserved Field Check**: `reserved == 0`.
4. **Header Consistency**: Conflicting flags or non-zero size on empty files are rejected with error code 8 (`HUFF_ERR_CORRUPT_HEADER`).
5. **Tree Traversal Bounds**:
   - Leaf count is strictly bounded to $\le 256$.
   - Stack recursion depth is bounded to $\le 256$ frames.
   - Leaf symbols must be unique. Duplicate leaf symbols trigger error code 9 (`HUFF_ERR_CORRUPT_TREE`).
6. **Premature EOF**: Any unexpected end of file during header, tree, or payload reading halts decompression with error code 11 (`HUFF_ERR_PREMATURE_EOF`).
7. **End-to-End CRC-32 Verification**: After decompressing `orig_size` bytes, the computed CRC-32 is checked against the header. Any bit corruption triggers error code 12 (`HUFF_ERR_CRC_MISMATCH`).
