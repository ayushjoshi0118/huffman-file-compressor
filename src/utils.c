#define _POSIX_C_SOURCE 200809L
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

/* Human readable error strings */
const char* huff_error_string(int err_code) {
    switch (err_code) {
        case HUFF_OK:
            return "Success";
        case HUFF_ERR_ARGS:
            return "Invalid command-line arguments";
        case HUFF_ERR_IO:
            return "File input/output error";
        case HUFF_ERR_FILE_NOT_FOUND:
            return "Input file not found or inaccessible";
        case HUFF_ERR_OUTPUT_EXISTS:
            return "Output file already exists";
        case HUFF_ERR_SAME_FILE:
            return "Input and output specify the same file";
        case HUFF_ERR_INVALID_MAGIC:
            return "Invalid file format: incorrect magic signature";
        case HUFF_ERR_UNSUPPORTED_VERSION:
            return "Unsupported format version";
        case HUFF_ERR_CORRUPT_HEADER:
            return "Corrupted or truncated file header";
        case HUFF_ERR_CORRUPT_TREE:
            return "Corrupted Huffman tree definition";
        case HUFF_ERR_CORRUPT_PAYLOAD:
            return "Corrupted compressed bitstream payload";
        case HUFF_ERR_PREMATURE_EOF:
            return "Premature end of file reached";
        case HUFF_ERR_CRC_MISMATCH:
            return "Data integrity check failed: CRC-32 checksum mismatch";
        case HUFF_ERR_MEMORY:
            return "Memory allocation failure";
        default:
            return "Unknown error";
    }
}

/* Endian-safe serialization (Big-Endian / Network Byte Order) */
uint16_t huff_read_u16_be(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) |
                      ((uint16_t)p[1]));
}

void huff_write_u16_be(uint8_t* p, uint16_t val) {
    p[0] = (uint8_t)((val >> 8) & 0xFF);
    p[1] = (uint8_t)(val & 0xFF);
}

uint32_t huff_read_u32_be(const uint8_t* p) {
    return (((uint32_t)p[0] << 24) |
            ((uint32_t)p[1] << 16) |
            ((uint32_t)p[2] << 8)  |
            ((uint32_t)p[3]));
}

void huff_write_u32_be(uint8_t* p, uint32_t val) {
    p[0] = (uint8_t)((val >> 24) & 0xFF);
    p[1] = (uint8_t)((val >> 16) & 0xFF);
    p[2] = (uint8_t)((val >> 8)  & 0xFF);
    p[3] = (uint8_t)(val & 0xFF);
}

uint64_t huff_read_u64_be(const uint8_t* p) {
    return (((uint64_t)p[0] << 56) |
            ((uint64_t)p[1] << 48) |
            ((uint64_t)p[2] << 40) |
            ((uint64_t)p[3] << 32) |
            ((uint64_t)p[4] << 24) |
            ((uint64_t)p[5] << 16) |
            ((uint64_t)p[6] << 8)  |
            ((uint64_t)p[7]));
}

void huff_write_u64_be(uint8_t* p, uint64_t val) {
    p[0] = (uint8_t)((val >> 56) & 0xFF);
    p[1] = (uint8_t)((val >> 48) & 0xFF);
    p[2] = (uint8_t)((val >> 40) & 0xFF);
    p[3] = (uint8_t)((val >> 32) & 0xFF);
    p[4] = (uint8_t)((val >> 24) & 0xFF);
    p[5] = (uint8_t)((val >> 16) & 0xFF);
    p[6] = (uint8_t)((val >> 8)  & 0xFF);
    p[7] = (uint8_t)(val & 0xFF);
}

/* File existence check */
bool huff_file_exists(const char* path) {
    if (!path) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && !S_ISDIR(st.st_mode));
#endif
}

/* Case-insensitive string comparison for portable file path matching */
static int huff_path_compare(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        /* Normalize path separators */
        if (c1 == '/') c1 = '\\';
        if (c2 == '/') c2 = '\\';
        /* Normalize ASCII uppercase to lowercase */
        if (c1 >= 'A' && c1 <= 'Z') c1 = (char)(c1 + 32);
        if (c2 >= 'A' && c2 <= 'Z') c2 = (char)(c2 + 32);
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/* Check if two paths point to the exact same file */
bool huff_is_same_file(const char* path1, const char* path2) {
    if (!path1 || !path2) return false;
    if (strcmp(path1, path2) == 0) return true;

#ifdef _WIN32
    char full1[MAX_PATH];
    char full2[MAX_PATH];
    DWORD len1 = GetFullPathNameA(path1, MAX_PATH, full1, NULL);
    DWORD len2 = GetFullPathNameA(path2, MAX_PATH, full2, NULL);
    if (len1 > 0 && len2 > 0 && huff_path_compare(full1, full2) == 0) {
        return true;
    }

    HANDLE h1 = CreateFileA(path1, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE h2 = CreateFileA(path2, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info1, info2;
        BOOL ok1 = GetFileInformationByHandle(h1, &info1);
        BOOL ok2 = GetFileInformationByHandle(h2, &info2);
        CloseHandle(h1);
        CloseHandle(h2);
        if (ok1 && ok2) {
            return (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber &&
                    info1.nFileIndexHigh == info2.nFileIndexHigh &&
                    info1.nFileIndexLow == info2.nFileIndexLow);
        }
    } else {
        if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
        if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
    }
#else
    struct stat st1, st2;
    if (stat(path1, &st1) == 0 && stat(path2, &st2) == 0) {
        return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
    }
#endif
    return false;
}

/* Get file size reliably */
int huff_get_file_size(const char* path, uint64_t* size_out) {
    if (!path || !size_out) return HUFF_ERR_ARGS;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }
    LARGE_INTEGER li;
    li.LowPart = fad.nFileSizeLow;
    li.HighPart = fad.nFileSizeHigh;
    *size_out = (uint64_t)li.QuadPart;
    return HUFF_OK;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return HUFF_ERR_FILE_NOT_FOUND;
    }
    *size_out = (uint64_t)st.st_size;
    return HUFF_OK;
#endif
}

/* Generate unique temp file path in the same directory */
bool huff_create_temp_path(const char* base_path, char* temp_path_out, size_t max_len) {
    if (!base_path || !temp_path_out || max_len == 0) return false;
    static unsigned int counter = 0;
#ifdef _WIN32
    int pid = (int)GetCurrentProcessId();
#else
    int pid = (int)getpid();
#endif
    int written = snprintf(temp_path_out, max_len, "%s.tmp_%d_%u", base_path, pid, ++counter);
    return (written > 0 && (size_t)written < max_len);
}

/* Memory management with active tracking */
static size_t g_active_allocations = 0;

void* huff_malloc(size_t size) {
    if (size == 0) return NULL;
    void* ptr = malloc(size);
    if (ptr) {
        g_active_allocations++;
    }
    return ptr;
}

void* huff_calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) return NULL;
    void* ptr = calloc(num, size);
    if (ptr) {
        g_active_allocations++;
    }
    return ptr;
}

void huff_free(void* ptr) {
    if (ptr) {
        if (g_active_allocations > 0) {
            g_active_allocations--;
        }
        free(ptr);
    }
}

size_t huff_get_active_allocations(void) {
    return g_active_allocations;
}

void huff_reset_memory_tracker(void) {
    g_active_allocations = 0;
}
