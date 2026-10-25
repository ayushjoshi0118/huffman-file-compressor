# C Systems Programming: Learning Guide & Code Tour

This guide highlights the core C systems programming concepts demonstrated throughout the Huffman File Compressor project, detailing their importance and pointing to their concrete implementation in the codebase.

---

## 1. Bit-Level Manipulation & Bitstream I/O
- **Concept**: C operates naturally on byte and word boundaries (`uint8_t`, `uint32_t`, `uint64_t`), whereas entropy encoding requires reading and writing individual bits.
- **Why it matters**: Packing variable-length bit codes (e.g. 3 bits, 7 bits, 15 bits) into byte streams without wasting space requires careful bitwise shifts, masks, and byte accumulation.
- **Where in code**:
  - [`include/bitstream.h`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/include/bitstream.h): Definition of `BitWriter` and `BitReader`.
  - [`src/bitstream.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/bitstream.c#L14-L45): `bitwriter_write_bit` and `bitwriter_write_code` accumulate bits MSB-first into `current_byte` and flush full bytes into a 64 KB memory buffer.
  - [`src/bitstream.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/bitstream.c#L93-L122): `bitreader_read_bit` unpacks individual bits from the buffered byte stream.

---

## 2. Dynamic Memory & Recursive Tree Data Structures
- **Concept**: Constructing and traversing binary trees using heap-allocated nodes connected via self-referential pointer structures.
- **Why it matters**: Tree structures represent non-linear relationships. In C, developers must manage memory lifecycles explicitly—every node allocated with `calloc`/`malloc` must be tracked and freed.
- **Where in code**:
  - [`include/huffman.h`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/include/huffman.h#L18-L28): `struct HuffmanNode` definition.
  - [`src/huffman.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/huffman.c#L36-L70): `huffman_create_leaf`, `huffman_create_internal`, and recursive post-order tree destruction in `huffman_free_tree`.
  - [`src/huffman.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/huffman.c#L173-L208): Recursive tree traversal generating prefix codes into a fixed-width table.

---

## 3. Min-Heap Priority Queue with Deterministic Total Ordering
- **Concept**: Implementing a binary min-heap array to maintain the lowest-frequency nodes efficiently in $O(\log N)$ time.
- **Why it matters**: A priority queue is central to greedy algorithms. Ensuring deterministic tie-breaking produces byte-for-byte reproducible output regardless of compiler or architecture.
- **Where in code**:
  - [`src/huffman.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/huffman.c#L73-L129): `minheap_push` (sift-up), `minheap_pop` (sift-down), and the multi-criteria comparator `compare_nodes`.

---

## 4. Endian-Safe Binary Serialization
- **Concept**: Never dumping raw C structs directly to disk with `fwrite(&struct, sizeof(struct), 1, fp)`.
- **Why it matters**: Struct padding, alignment requirements, and CPU endianness (Little-Endian on x86/x64 vs Big-Endian on network/embedded architectures) make struct dumps non-portable and vulnerable to corruption.
- **Where in code**:
  - [`src/utils.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/utils.c#L46-L94): `huff_read_u16_be`, `huff_write_u16_be`, `huff_read_u32_be`, `huff_write_u32_be`, `huff_read_u64_be`, and `huff_write_u64_be`.
  - [`src/file_format.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/file_format.c#L40-L103): Explicit field-by-field serialization and deserialization of the file header.

---

## 5. Robust File I/O & Atomic File Replacement
- **Concept**: Working with standard binary I/O, buffered transfers, and atomic replacement patterns.
- **Why it matters**: Writing directly to the target output file can leave corrupted partial files on disk if the disk fills up or an error occurs mid-stream. Atomic replacement ensures the target file is updated only upon 100% successful completion.
- **Where in code**:
  - [`src/utils.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/utils.c#L162-L174): `huff_create_temp_path` generates unique temporary filenames using process IDs and counters.
  - [`src/compressor.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/compressor.c#L69-L177): Writing output to a temporary file, flushing buffers, closing, and performing an atomic `rename` upon success (or `remove` on error).

---

## 6. Same-File & Overwrite Protection
- **Concept**: Checking whether two paths refer to the same physical file on disk across operating systems.
- **Why it matters**: If a user runs `huff -c file.txt file.txt`, writing to the output while reading the input destroys data.
- **Where in code**:
  - [`src/utils.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/utils.c#L112-L148): `huff_is_same_file` normalizes paths with `GetFullPathName` and case-insensitive comparison, and inspects volume serial numbers and file index IDs via `GetFileInformationByHandle` on Windows (and `st_ino`/`st_dev` on POSIX).
  - [`src/main.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/main.c#L248-L265): CLI checks rejecting same-file invocations and prompting interactively before overwriting existing files.

---

## 7. Memory Leak Tracking & Clean Exit Paths
- **Concept**: Tracking dynamic allocations and ensuring every resource (file handles, heap buffers, tree nodes) is freed on all exit paths.
- **Why it matters**: Production C utilities must remain leak-free over continuous, repeated execution cycles.
- **Where in code**:
  - [`src/utils.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/src/utils.c#L176-L208): Memory wrapper functions `huff_malloc`, `huff_calloc`, and `huff_free` tracking `g_active_allocations`.
  - [`tests/unit_tests.c`](file:///c:/Users/ayush/Dropbox/PC%20(2)/Downloads/file%20compression/tests/unit_tests.c#L240-L245): Automated verification asserting `huff_get_active_allocations() == 0`.
