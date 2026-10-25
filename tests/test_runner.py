#!/usr/bin/env python3
"""
Comprehensive Automated Test Suite for Huffman File Compressor (C11)
Validates round-trip byte-for-byte exactness, edge cases, corruption resistance,
CLI safety protections, and performance across all file types.
"""

import os
import sys
import subprocess
import hashlib
import tempfile
import random
import shutil

EXECUTABLE = os.path.abspath("./huff.exe")
TEST_DIR = os.path.abspath("./tests_tmp")

passed_tests = 0
failed_tests = 0
total_tests = 0

def log_test(name, passed, message=""):
    global passed_tests, failed_tests, total_tests
    total_tests += 1
    if passed:
        passed_tests += 1
        print(f"  [PASS] {name} {message}")
    else:
        failed_tests += 1
        print(f"  [FAIL] {name}: {message}")

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def run_huff(args):
    cmd = [EXECUTABLE] + args
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return res.returncode, res.stdout, res.stderr

def run_roundtrip_test(name, data_bytes, extra_flags=[]):
    in_path = os.path.join(TEST_DIR, f"{name}_orig.bin")
    huff_path = os.path.join(TEST_DIR, f"{name}.huff")
    restored_path = os.path.join(TEST_DIR, f"{name}_restored.bin")

    with open(in_path, "wb") as f:
        f.write(data_bytes)

    orig_hash = sha256_file(in_path)

    # Compress
    code, stdout, stderr = run_huff(["-c", in_path, huff_path, "-f"] + extra_flags)
    if code != 0:
        log_test(name, False, f"Compression failed with exit code {code}: {stderr.strip()}")
        return False

    # Decompress
    code, stdout, stderr = run_huff(["-d", huff_path, restored_path, "-f"])
    if code != 0:
        log_test(name, False, f"Decompression failed with exit code {code}: {stderr.strip()}")
        return False

    restored_hash = sha256_file(restored_path)
    if orig_hash != restored_hash:
        log_test(name, False, f"Hash mismatch! Orig: {orig_hash}, Restored: {restored_hash}")
        return False

    # Check file sizes
    orig_size = len(data_bytes)
    huff_size = os.path.getsize(huff_path)
    log_test(name, True, f"({orig_size} bytes -> {huff_size} bytes compressed, hash verified)")
    return True

def main():
    if not os.path.isfile(EXECUTABLE):
        print(f"Error: Executable '{EXECUTABLE}' not found. Build it first.")
        sys.exit(1)

    if os.path.exists(TEST_DIR):
        shutil.rmtree(TEST_DIR)
    os.makedirs(TEST_DIR, exist_ok=True)

    print("================================================================")
    print("      HUFFMAN FILE COMPRESSOR AUTOMATED VERIFICATION SUITE       ")
    print("================================================================\n")

    # 1. Round-Trip Correctness Tests
    print("--- 1. Round-Trip Byte-for-Byte Correctness Tests ---")

    # 1.1 Empty File
    run_roundtrip_test("empty_file", b"")

    # 1.2 Single Byte Files (Different Byte Values)
    run_roundtrip_test("single_byte_ascii", b"Z")
    run_roundtrip_test("single_byte_null", b"\x00")
    run_roundtrip_test("single_byte_ff", b"\xFF")
    run_roundtrip_test("single_byte_55", b"\x55")

    # 1.3 Repeated Single Byte (2 Megabytes & 10 Megabytes)
    run_roundtrip_test("repeated_2mb", b"A" * 2_000_000)
    run_roundtrip_test("repeated_10mb", b"K" * 10_000_000)

    # 1.4 All 256 Byte Values
    all_256 = bytes(range(256))
    run_roundtrip_test("all_256_bytes", all_256)

    # 1.5 Repeated All 256 Bytes (Permutations)
    all_256_repeated = (all_256 * 100) + bytes(reversed(range(256))) * 50
    run_roundtrip_test("all_256_repeated", all_256_repeated)

    # 1.6 Plain English Text
    english_text = (
        "The quick brown fox jumps over the lazy dog. "
        "Huffman coding is a particular type of optimal prefix code that is commonly used for lossless data compression. "
        "The process of finding or using such a code proceeds by means of Huffman's algorithm, developed by David A. Huffman "
        "while he was a Sc.D. student at MIT, and published in the 1952 paper 'A Method for the Construction of Minimum-Redundancy Codes'.\n"
    ) * 200
    run_roundtrip_test("english_prose", english_text.encode("utf-8"))

    # 1.7 C Source Code File
    with open("src/compressor.c", "rb") as f:
        c_source_bytes = f.read()
    run_roundtrip_test("c_source_code", c_source_bytes)

    # 1.8 UTF-8 Multilingual & Emoji Text
    utf8_text = (
        "Hello, World! — Καλημέρα κόσμε! — Привет, мир! — こんにちは世界！ — "
        "你好，世界！ — 🌍🚀🎉🔥✨💻 — Résumé, naïve, façade, über, café.\n"
    ) * 300
    run_roundtrip_test("utf8_multilingual", utf8_text.encode("utf-8"))

    # 1.9 Skewed Frequency Distribution (Zipfian)
    skewed_bytes = bytearray()
    skewed_bytes.extend(b'e' * 50000)
    skewed_bytes.extend(b't' * 30000)
    skewed_bytes.extend(b'a' * 25000)
    skewed_bytes.extend(b'o' * 20000)
    skewed_bytes.extend(b'i' * 18000)
    skewed_bytes.extend(b'n' * 15000)
    skewed_bytes.extend(b'z' * 100)
    skewed_bytes.extend(b'q' * 50)
    run_roundtrip_test("skewed_frequencies", bytes(skewed_bytes))

    # 1.10 Random Binary Data (High Entropy)
    random.seed(42)
    random_bytes = bytearray(random.getrandbits(8) for _ in range(128 * 1024))
    run_roundtrip_test("random_high_entropy", bytes(random_bytes))

    # 1.11 Embedded Null Bytes
    embedded_nulls = b"Start\x00Middle\x00\x00\x00\x01\x02\x00End\x00" * 1000
    run_roundtrip_test("embedded_null_bytes", embedded_nulls)

    # 1.12 Large File (5 Megabytes Mixed Data)
    print("\n--- 2. Large File Performance Test (5 MB) ---")
    large_buf = bytearray()
    for _ in range(50):
        large_buf.extend(english_text.encode("utf-8")[:50000])
        large_buf.extend(bytes(range(256)) * 100)
    large_data = bytes(large_buf)[:5_000_000]
    run_roundtrip_test("large_5mb_mixed", large_data, extra_flags=["-b"])

    # 2. CLI Edge Cases & Safety Protections
    print("\n--- 3. CLI Safety & Edge Cases ---")

    # 2.1 Missing input file
    code, out, err = run_huff(["-c", "non_existent_file_xyz.txt", os.path.join(TEST_DIR, "out.huff")])
    log_test("missing_input_file_rejected", code != 0, f"Exit code {code} (expected non-zero)")

    # 2.2 Same input and output file path
    same_path = os.path.join(TEST_DIR, "same.txt")
    with open(same_path, "wb") as f:
        f.write(b"data")
    code, out, err = run_huff(["-c", same_path, same_path])
    log_test("same_file_detected_and_rejected", code == 5, f"Exit code {code} (HUFF_ERR_SAME_FILE)")

    # 2.3 Overwrite protection without -f
    target_out = os.path.join(TEST_DIR, "existing_out.huff")
    with open(target_out, "wb") as f:
        f.write(b"already exists")
    code, out, err = run_huff(["-c", same_path, target_out])
    log_test("overwrite_protection_active", code == 4, f"Exit code {code} (HUFF_ERR_OUTPUT_EXISTS)")

    # 2.4 Force overwrite with -f
    code, out, err = run_huff(["-c", same_path, target_out, "-f"])
    log_test("force_overwrite_allowed", code == 0, f"Exit code {code}")

    # 2.5 Invalid arguments
    code, out, err = run_huff(["--invalid-switch-123"])
    log_test("invalid_arguments_rejected", code != 0, f"Exit code {code}")

    # 2.6 Info mode execution
    code, out, err = run_huff(["-i", same_path])
    log_test("info_mode_execution", code == 0 and "Shannon entropy" in out, "Statistics generated successfully")

    # 2.7 Help options (-h, --help)
    code_h, out_h, _ = run_huff(["-h"])
    code_help, out_help, _ = run_huff(["--help"])
    log_test("help_flags_supported", code_h == 0 and code_help == 0 and "Usage:" in out_h, "Help output verified")

    # 2.8 Version options (--version, -V, -v sole)
    code_v1, out_v1, _ = run_huff(["--version"])
    code_v2, out_v2, _ = run_huff(["-V"])
    code_v3, out_v3, _ = run_huff(["-v"])
    log_test("version_flags_supported", code_v1 == 0 and code_v2 == 0 and code_v3 == 0 and "huff version" in out_v1, "Version output verified")

    # 2.9 Verbose mode (-v with -c)
    huff_v_path = os.path.join(TEST_DIR, "verbose_test.huff")
    code_verb, out_verb, _ = run_huff(["-c", same_path, huff_v_path, "-v", "-f"])
    log_test("verbose_mode_output", code_verb == 0 and "Huffman Code Lengths:" in out_verb, "Verbose code table verified")

    # 2.10 Benchmark mode (-b)
    code_bench, out_bench, _ = run_huff(["-c", same_path, huff_v_path, "-b", "-f"])
    log_test("benchmark_mode_timing", code_bench == 0 and "Elapsed time:" in out_bench, "Timing output verified")

    # 2.11 Built-in Self-Test flag (--self-test)
    code_self, out_self, _ = run_huff(["--self-test"])
    log_test("cli_self_test_flag", code_self == 0 and "All internal self-tests passed" in out_self, "Built-in self-test verified")

    # 2.12 Invalid output directory handling
    code_inv_dir, _, _ = run_huff(["-c", same_path, os.path.join("non_existent_folder_abc", "out.huff")])
    log_test("invalid_output_directory_rejected", code_inv_dir == 2, f"Exit code {code_inv_dir} (HUFF_ERR_IO)")

    # 3. File Format Corruption & Security Resistance
    print("\n--- 4. Malformed Input & Corruption Resistance ---")

    valid_huff_path = os.path.join(TEST_DIR, "english_prose.huff")
    with open(valid_huff_path, "rb") as f:
        valid_huff_bytes = f.read()

    corrupt_out = os.path.join(TEST_DIR, "corrupt_restored.txt")

    # 3.1 Bad Magic Bytes
    bad_magic = b"NOPE" + valid_huff_bytes[4:]
    bad_magic_file = os.path.join(TEST_DIR, "bad_magic.huff")
    with open(bad_magic_file, "wb") as f:
        f.write(bad_magic)
    code, out, err = run_huff(["-d", bad_magic_file, corrupt_out, "-f"])
    log_test("corrupted_magic_rejected", code == 6, f"Exit code {code} (HUFF_ERR_INVALID_MAGIC)")

    # 3.2 Unsupported Version
    bad_version = valid_huff_bytes[:4] + bytes([99]) + valid_huff_bytes[5:]
    bad_version_file = os.path.join(TEST_DIR, "bad_version.huff")
    with open(bad_version_file, "wb") as f:
        f.write(bad_version)
    code, out, err = run_huff(["-d", bad_version_file, corrupt_out, "-f"])
    log_test("unsupported_version_rejected", code == 7, f"Exit code {code} (HUFF_ERR_UNSUPPORTED_VERSION)")

    # 3.3 Non-Zero Reserved Field
    bad_res = bytearray(valid_huff_bytes)
    bad_res[6] = 0x12
    bad_res[7] = 0x34
    bad_res_file = os.path.join(TEST_DIR, "bad_reserved.huff")
    with open(bad_res_file, "wb") as f:
        f.write(bytes(bad_res))
    code, out, err = run_huff(["-d", bad_res_file, corrupt_out, "-f"])
    log_test("nonzero_reserved_field_rejected", code == 8, f"Exit code {code} (HUFF_ERR_CORRUPT_HEADER)")

    # 3.4 Zero Size Without Empty File Flag
    bad_zero = bytearray(valid_huff_bytes)
    bad_zero[5] = 0x04 # HAS_CRC32 only, not EMPTY_FILE
    for i in range(8, 16): bad_zero[i] = 0 # orig_size = 0
    bad_zero_file = os.path.join(TEST_DIR, "bad_zero_size.huff")
    with open(bad_zero_file, "wb") as f:
        f.write(bytes(bad_zero))
    code, out, err = run_huff(["-d", bad_zero_file, corrupt_out, "-f"])
    log_test("zero_size_without_empty_flag_rejected", code == 8, f"Exit code {code} (HUFF_ERR_CORRUPT_HEADER)")

    # 3.5 Corrupted CRC-32 Header Value (Tampered Checksum Only)
    bad_crc = bytearray(valid_huff_bytes)
    bad_crc[16] ^= 0xAA # Invert header CRC32 bytes
    bad_crc[17] ^= 0x55
    bad_crc_file = os.path.join(TEST_DIR, "bad_crc.huff")
    with open(bad_crc_file, "wb") as f:
        f.write(bytes(bad_crc))
    code, out, err = run_huff(["-d", bad_crc_file, corrupt_out, "-f"])
    log_test("header_crc_tamper_detected", code == 12, f"Exit code {code} (HUFF_ERR_CRC_MISMATCH)")

    # 3.6 Truncated Header (e.g. only 8 bytes)
    trunc_header = valid_huff_bytes[:8]
    trunc_header_file = os.path.join(TEST_DIR, "trunc_header.huff")
    with open(trunc_header_file, "wb") as f:
        f.write(trunc_header)
    code, out, err = run_huff(["-d", trunc_header_file, corrupt_out, "-f"])
    log_test("truncated_header_rejected", code in [8, 11], f"Exit code {code} (Header corrupt / premature EOF)")

    # 3.7 Truncated Single-Symbol Header (15 bytes instead of 21)
    single_sym_path = os.path.join(TEST_DIR, "single_byte_ascii.huff")
    with open(single_sym_path, "rb") as f:
        single_sym_bytes = f.read()
    trunc_single = single_sym_bytes[:15]
    trunc_single_file = os.path.join(TEST_DIR, "trunc_single.huff")
    with open(trunc_single_file, "wb") as f:
        f.write(trunc_single)
    code, out, err = run_huff(["-d", trunc_single_file, corrupt_out, "-f"])
    log_test("truncated_single_symbol_header_rejected", code in [8, 11], f"Exit code {code} (Header corrupt / premature EOF)")

    # 3.8 Truncated Payload
    trunc_payload = valid_huff_bytes[:len(valid_huff_bytes) // 2]
    trunc_payload_file = os.path.join(TEST_DIR, "trunc_payload.huff")
    with open(trunc_payload_file, "wb") as f:
        f.write(trunc_payload)
    code, out, err = run_huff(["-d", trunc_payload_file, corrupt_out, "-f"])
    log_test("truncated_payload_safely_rejected", code != 0, f"Exit code {code} (Premature EOF detected)")

    # 3.9 Bit-Flipped Payload (Data Integrity / CRC-32 Checksum Mismatch Detection)
    flipped = bytearray(valid_huff_bytes)
    flipped[-10] ^= 0xFF
    flipped_file = os.path.join(TEST_DIR, "flipped_payload.huff")
    with open(flipped_file, "wb") as f:
        f.write(bytes(flipped))
    code, out, err = run_huff(["-d", flipped_file, corrupt_out, "-f"])
    log_test("tampered_payload_crc_mismatch_detected", code in [10, 11, 12], f"Exit code {code} (Integrity / CRC check)")

    # 3.10 Malformed Tree with Infinite Internal Nodes (All 0 bits)
    malformed_tree = bytearray(valid_huff_bytes[:20]) # Base 20-byte header
    malformed_tree.extend(b"\x00" * 500) # 0 bits meaning left, left, left...
    malformed_tree_file = os.path.join(TEST_DIR, "infinite_tree.huff")
    with open(malformed_tree_file, "wb") as f:
        f.write(bytes(malformed_tree))
    code, out, err = run_huff(["-d", malformed_tree_file, corrupt_out, "-f"])
    log_test("malformed_infinite_tree_safely_rejected", code in [9, 11], f"Exit code {code} (Corrupt tree / premature EOF)")

    # 4. Successive Repeated Operations
    print("\n--- 5. Successive Repeated Runs (Stability) ---")
    successive_ok = True
    for cycle in range(5):
        s_in = os.path.join(TEST_DIR, f"cycle_{cycle}.txt")
        s_huff = os.path.join(TEST_DIR, f"cycle_{cycle}.huff")
        s_out = os.path.join(TEST_DIR, f"cycle_{cycle}_out.txt")
        test_payload = f"Cycle {cycle} content: " + ("Random payload " * 50)
        with open(s_in, "w") as f:
            f.write(test_payload)
        c_code, _, _ = run_huff(["-c", s_in, s_huff, "-f"])
        d_code, _, _ = run_huff(["-d", s_huff, s_out, "-f"])
        if c_code != 0 or d_code != 0 or sha256_file(s_in) != sha256_file(s_out):
            successive_ok = False
            break
    log_test("successive_cycles_stability", successive_ok, "5 consecutive cycles without error")

    # Cleanup temporary test directory
    shutil.rmtree(TEST_DIR, ignore_errors=True)

    print("\n================================================================")
    print(f"TEST RESULTS: {passed_tests} PASSED, {failed_tests} FAILED out of {total_tests} tests.")
    print("================================================================")

    if failed_tests > 0:
        sys.exit(1)
    else:
        print("\nAll integration and security verification tests PASSED!\n")
        sys.exit(0)

if __name__ == "__main__":
    main()
