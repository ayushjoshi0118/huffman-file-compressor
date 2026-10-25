# Changelog

All notable changes to the **Huffman File Compressor** project are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-10-25

### Added
- **Core Engine (C11 Standard Library Only)**:
  - 2-pass Huffman file compression and exact lossless decompression.
  - Byte-frequency counting supporting all 256 byte values and embedded null bytes.
  - Custom binary min-heap priority queue with deterministic 4-tier tie-breaking.
  - Canonical bitstream tree serialization (pre-order traversal, 25 to 320 bytes overhead).
  - Bit-level I/O (`BitWriter` and `BitReader`) with 64 KB memory buffering and MSB-first packing.
  - IEEE 802.3 standard CRC-32 integrity computation and verification.
- **Custom Versioned Binary Format (HUFFv1)**:
  - 20-byte portable header: Magic (`HUFF`), version (`1`), flags, reserved field, 64-bit uncompressed size, and 32-bit CRC-32.
  - All integers encoded strictly in Big-Endian (Network Byte Order).
  - Dedicated flag handling for 0-byte empty files (`HUFF_FLAG_EMPTY_FILE`).
  - Dedicated flag handling for single-symbol files (`HUFF_FLAG_SINGLE_SYMBOL`).
- **Command-Line Interface**:
  - `-c, --compress`: File compression with automatic statistics report.
  - `-d, --decompress`: Lossless decompression with CRC-32 verification.
  - `-i, --info`: Shannon entropy calculation and detailed byte frequency analysis.
  - `-f, --force`: Non-interactive output file overwrite flag.
  - `-v, --verbose`: Detailed Huffman code table and bit length output.
  - `-b, --benchmark`: High-resolution execution timing.
  - `--self-test`: Built-in runtime algorithmic self-verification.
  - `--help, -h` and `--version, -V` documentation commands.
- **Safety & Portability**:
  - Same-file detection rejecting identical input and output paths (`HUFF_ERR_SAME_FILE`).
  - Interactive confirmation prompt before overwriting existing files in terminal sessions.
  - Atomic temporary file creation and replacement to prevent corrupted output on disk failures.
  - Defensive recursion and leaf-count bounds preventing stack overflows on malformed inputs.
  - Comprehensive memory tracker verifying zero memory leaks (`huff_get_active_allocations() == 0`).
- **Build System & Test Automation**:
  - Universal `Makefile` compatible with GNU Make, MinGW (`mingw32-make`), and POSIX.
  - Native Windows PowerShell build script (`build.ps1`).
  - C unit test suite (`tests/unit_tests.c`) verifying endianness, bitstreams, heaps, trees, headers, and memory.
  - Automated integration test suite (`tests/test_runner.py`) running 24 comprehensive test scenarios.
