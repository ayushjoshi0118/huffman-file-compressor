# Huffman File Compressor

A complete, production-grade command-line file compression and decompression utility implemented from scratch in pure C (C11 standard library only).

The tool implements optimal prefix-free **Huffman coding** across all 256 byte values, featuring a versioned binary file format, bit-level I/O buffering, deterministic min-heap priority queues, IEEE 802.3 CRC-32 integrity validation, atomic output file replacement, and comprehensive defensive error handling.

---

## Table of Contents
1. [Key Features](#key-features)
2. [Prerequisites](#prerequisites)
3. [Build Instructions](#build-instructions)
4. [Usage & Commands](#usage--commands)
5. [Example Terminal Output](#example-terminal-output)
6. [Architecture & Technical Principles](#architecture--technical-principles)
   - [Huffman Coding Theory](#huffman-coding-theory)
   - [Deterministic Min-Heap Priority Queue](#deterministic-min-heap-priority-queue)
   - [Bitstream Packing & Buffering](#bitstream-packing--buffering)
   - [Custom Compressed Format (HUFFv1)](#custom-compressed-format-huffv1)
7. [Complexity Analysis](#complexity-analysis)
8. [Automated Testing & Verification](#automated-testing--verification)
9. [Tested Environments & Known Limitations](#tested-environments--known-limitations)
10. [Troubleshooting](#troubleshooting)

---

## Key Features

- **Pure C11 Standard Library**: No external dependencies or non-standard third-party libraries.
- **Arbitrary Binary Support**: Correctly processes any file type: plain text, C source code, CSVs, executable binaries, images, and files with embedded null bytes (`0x00`).
- **Exact Byte-for-Byte Restoration**: Lossless decompression guaranteed with SHA-256 and CRC-32 verification.
- **Portable Binary Specification**: Network byte order (Big-Endian) serialization prevents platform endianness or compiler struct padding issues.
- **Deterministic Code Generation**: Total ordering on min-heap tie-breaking ensures identical compressed bits across compilers and platforms.
- **Compact Tree Serialization**: In-stream pre-order traversal serializes Huffman trees in just 25 to 320 bytes.
- **Special Edge-Case Optimizations**:
  - Empty files (0 bytes) are encoded with a 20-byte metadata-only header.
  - Single-symbol files (e.g. 5 MB of repeated `'A'`s) compress to 21 bytes total.
- **Defensive Systems Programming**:
  - Same-file detection prevents overwriting source files (`huff -c file file`).
  - Interactive confirmation prevents accidental overwrites; non-interactive `-f/--force` flag for scripting.
  - Atomic file writing (via unique `.tmp` files) prevents leaving corrupt files on disk interruptions.
  - Bounded recursion and duplicate symbol detection mitigate malformed or malicious inputs.
  - Active memory tracker ensuring zero memory leaks.

---

## Prerequisites

- **C Compiler**: Any standard C11 compatible compiler (`gcc`, `clang`, or MSVC).
- **Build Tools**:
  - Windows: MinGW (`gcc`, `mingw32-make`) or PowerShell (via `build.ps1`).
  - Linux / macOS: `gcc` or `clang` and standard GNU `make`.
- **Testing (Optional)**: Python 3.8+ for automated integration test orchestration.

---

## Build Instructions

### Using Make (Linux, macOS, MinGW)

Build the release executable:
```bash
make
# or on Windows with MinGW:
mingw32-make
```

Build with debug symbols and assertions:
```bash
make debug
# or on Windows:
mingw32-make debug
```

Run all unit tests and integration tests:
```bash
make test
# or on Windows:
mingw32-make test
```

Clean build artifacts:
```bash
make clean
# or on Windows:
mingw32-make clean
```

### Using PowerShell (Windows Native)

If GNU Make is not installed, use the included native PowerShell build script:

```powershell
# Build release executable
.\build.ps1

# Run unit tests and integration suite
.\build.ps1 -Target test

# Clean artifacts
.\build.ps1 -Target clean
```

---

## Usage & Commands

```
huff -c <input> <output> [options]    Compress input file into output
huff -d <input> <output> [options]    Decompress input file into output
huff -i <input>                       Analyze and display file byte frequencies
huff --help | -h                      Show help message
huff --version | -V                   Show version information
```

### Options & Flags
- `-f, --force`: Overwrite existing output files without prompting.
- `-v, --verbose`: Display Huffman tree code lengths and byte distributions.
- `-b, --benchmark`: Measure and print execution time.
- `--self-test`: Run internal algorithm verification tests.

### Common Examples

1. **Compress a text or binary file**:
   ```bash
   huff -c document.txt document.huff
   ```

2. **Decompress a `.huff` archive**:
   ```bash
   huff -d document.huff restored.txt
   ```

3. **Inspect file entropy and byte frequencies**:
   ```bash
   huff -i document.txt
   ```

4. **Compress with verbose tree codes and benchmark timing**:
   ```bash
   huff -c data.csv data.huff -v -b -f
   ```

5. **Execute internal algorithm self-test**:
   ```bash
   huff --self-test
   ```

---

## Example Terminal Output

### 1. Compression Output
```
$ huff -c war_and_peace.txt war_and_peace.huff

--- Huffman Compression Statistics ---
Original size:     3289052 bytes
Compressed size:   1865241 bytes (including metadata)
Distinct symbols:  89 of 256
Data CRC-32:       0x4A8F21BC
Compression ratio: 56.71% (compressed / original)
Space savings:     43.29%
Result:            Saved 1423811 bytes (file is smaller)
Elapsed time:      0.0820 seconds
---------------------------------------
```

### 2. Decompression Output
```
$ huff -d war_and_peace.huff restored.txt -f

--- Huffman Decompression Statistics ---
Compressed size:   1865241 bytes
Restored size:     3289052 bytes
Expected CRC-32:   0x4A8F21BC
Calculated CRC-32: 0x4A8F21BC (MATCH)
Elapsed time:      0.0710 seconds
Integrity status:  VERIFIED (100% byte-for-byte lossless)
-----------------------------------------
```

### 3. File Info & Frequency Analysis (`-i`)
```
$ huff -i sample.txt

=== File Information & Frequency Analysis ===
File path:         sample.txt
Total size:        80600 bytes
Distinct symbols:  45 / 256
Shannon entropy:   4.4612 bits/byte (theoretical min: 43.89 KB)
CRC-32 checksum:   0x5D842C39

Top byte frequencies:
Symbol   Hex / Char   Count        Frequency 
---------------------------------------------
32       ' ' (0x20)   13400         16.63%
101      'e' (0x65)   9600          11.91%
116      't' (0x74)   7400           9.18%
97       'a' (0x61)   6200           7.69%
111      'o' (0x6F)   5800           7.20%
105      'i' (0x69)   5400           6.70%
110      'n' (0x6E)   5000           6.20%
... and 29 more distinct symbols.
=============================================
```

---

## Architecture & Technical Principles

### Huffman Coding Theory
Huffman coding constructs a variable-length code table for encoding source symbols based on their estimated probabilities. More frequent symbols receive shorter bit codes, while less frequent symbols receive longer codes, subject to the **prefix-free condition** (no code is a prefix of any other code).

### Deterministic Min-Heap Priority Queue
To generate the optimal code tree, the leaf nodes are placed into a binary min-heap. Nodes are extracted two at a time and combined into an internal node whose weight is the sum of their frequencies.

To guarantee that the generated codes are 100% deterministic and reproducible across all platforms, tie-breaking between nodes with identical frequencies is resolved using a strict 4-tier comparison:
1. Frequency (ascending)
2. Minimum Symbol in subtree (ascending)
3. Node Type (leaf before internal)
4. Sequential Creation Order ID (ascending)

### Bitstream Packing & Buffering
Processors read and write bytes, but Huffman codes consist of arbitrary bit sequences (e.g. 3 bits, 7 bits, 12 bits).
- `BitWriter`: Packs bits MSB-first into a byte accumulator. Filled bytes are deposited into a 64 KB memory buffer (`HUFF_BUFFER_SIZE`) and written to disk in bulk.
- `bitwriter_flush`: When input ends, any incomplete byte is padded with `0`s and written out.
- `BitReader`: Pulls 64 KB blocks from disk and extracts bits MSB-first.
- **Zero-Ambiguity Termination**: The decompressor decodes symbols until `bytes_decoded == header.original_size`, which safely ignores any trailing zero padding bits without creating spurious output bytes.

### Custom Compressed Format (HUFFv1)
All files encoded with this tool follow the specification detailed in [`docs/FILE_FORMAT.md`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/docs/FILE_FORMAT.md):

```
+-------------------------------------------------------------+
| 4B Magic  | 1B Ver | 1B Flags | 2B Reserved | 8B Orig Size  |
| "HUFF"    |  0x01  |  Bitmap  |    0x0000   |  (Big-Endian) |
+-------------------------------------------------------------+
| 4B CRC-32 (Big-Endian) | [1B Single Symbol if flag set]     |
+-------------------------------------------------------------+
| Serialized Huffman Tree (Pre-order traversal bits)          |
+-------------------------------------------------------------+
| Encoded Payload Bitstream                                   |
+-------------------------------------------------------------+
```

---

## Complexity Analysis

| Phase | Time Complexity | Memory Complexity | Notes |
| :--- | :--- | :--- | :--- |
| **Pass 1 (Frequencies & CRC)** | $O(M)$ | $O(1)$ | 64 KB streaming buffer |
| **Min-Heap Construction** | $O(K \log K)$ | $O(K)$ | Alphabet size $K \le 256$ |
| **Tree Construction** | $O(K \log K)$ | $O(K)$ | At most 511 nodes |
| **Code Generation** | $O(K \cdot D)$ | $O(K)$ | Depth $D \le 256$ |
| **Pass 2 (Payload Encoding)** | $O(M)$ | $O(1)$ | Direct array lookup |
| **Decompression** | $O(M)$ | $O(K)$ tree + $O(1)$ | Traverses tree from root |

Where $M$ is the file size in bytes, and $K$ is the number of distinct byte values in the alphabet ($0 \le K \le 256$).

---

## Automated Testing & Verification

The project includes two layers of automated testing:

1. **C Unit Tests (`tests/unit_tests.c`)**:
   - Big-Endian serialization and deserialization.
   - IEEE 802.3 CRC-32 test vectors.
   - BitWriter and BitReader roundtrip bit streaming.
   - 32-bit unsigned bit reading with sign preservation (`int64_t`).
   - Min-heap sorting and deterministic tie-breaking.
   - All 256 symbols tree construction and prefix-free code verification.
   - Tree serialization and deserialization bit roundtrip.
   - Single-symbol leaf tree serialization/deserialization.
   - Header validation, consistency checks, and corruption rejection.
   - Dynamic memory leak verification (`active_allocations == 0`).
   - Total: **10 / 10 passed (100%)**.

2. **End-to-End Integration Tests (`tests/test_runner.py`)**:
   - Empty file (0 bytes).
   - Single-byte files across diverse byte values (`'Z'`, `0x00`, `0x55`, `0xFF`).
   - Repeated single-byte files (2 Megabytes and 10 Megabytes).
   - All 256 byte values and permutations.
   - Plain English text prose.
   - C source code file.
   - Multilingual UTF-8 text (English, Greek, Russian, Japanese, Chinese, Emoji).
   - High-entropy pseudo-random binary data.
   - Highly skewed frequency distribution (Zipfian).
   - Embedded null bytes (`0x00`).
   - Large 5 Megabyte mixed file.
   - Non-existent input file rejection.
   - Same-file input/output rejection (`huff -c f f`).
   - Overwrite protection enforcement without `-f`.
   - Force overwrite execution with `-f`.
   - Invalid arguments rejection.
   - Info mode Shannon entropy analysis (`-i`).
   - Help flags (`-h`, `--help`).
   - Version flags (`-v` sole, `-V`, `--version`).
   - Verbose mode code lengths output (`-v`).
   - Benchmark mode timing statistics (`-b`).
   - CLI self-test execution (`--self-test`).
   - Invalid output directory rejection (`HUFF_ERR_IO`).
   - Corrupted magic signature rejection.
   - Unsupported format version rejection.
   - Non-zero reserved field rejection.
   - Zero-size header without empty flag rejection.
   - Corrupted header CRC-32 checksum rejection.
   - Truncated base header rejection.
   - Truncated single-symbol header rejection.
   - Truncated payload premature EOF rejection.
   - Bit-flipped payload CRC-32 mismatch detection.
   - Malformed infinite internal node tree rejection.
   - Successive repeated runs stability (5 consecutive cycles).
   - Total: **39 / 39 passed (100%)**.

Run tests at any time with:
```bash
python tests/test_runner.py
```

---

## Tested Environments & Known Limitations

### Verified Platforms
- **Windows 10 / 11 (x86_64)**: MinGW GCC 6.3.0 (`-std=c11 -Wall -Wextra -Werror -pedantic -O2`). Tested and verified with 100% passing tests.
- **POSIX Architecture**: Designed for GCC 7+ / Clang on Linux and macOS.

### Known Limitations
- **Entropy Coding Bounds**: In accordance with Shannon's Source Coding Theorem, files with maximum entropy (such as already-compressed `.zip`, `.gz`, `.mp4`, or encrypted files) cannot be compressed further using Huffman coding. Small files may grow slightly due to the 20-byte header and tree metadata overhead.
- **Static 2-Pass Requirement**: The compressor performs Pass 1 (frequency counting) followed by Pass 2 (encoding). Non-seekable stream pipes (`stdin`/`stdout`) are not supported directly in the current version.

---

## Troubleshooting

- **Error: `Output file already exists`**:
  Use the `-f` or `--force` flag to confirm that you want to overwrite the destination file.
- **Error: `Data integrity check failed: CRC-32 checksum mismatch`**:
  The compressed file has suffered byte tampering or disk corruption.
- **Error: `Input and output specify the same file`**:
  Specify a distinct output path to prevent overwriting your original file.

---

## License

MIT License. Designed for education, systems software architecture, and real-world utility.
