# PowerShell build script for Huffman File Compressor

param (
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

$CC = "gcc"
$CFLAGS = @("-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2", "-Iinclude")
$DEBUG_FLAGS = @("-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g", "-DDEBUG", "-Iinclude")

$SOURCES = @(
    "src/utils.c",
    "src/checksum.c",
    "src/bitstream.c",
    "src/huffman.c",
    "src/file_format.c",
    "src/compressor.c",
    "src/decompressor.c"
)

function Build-Huff([string[]]$flags) {
    Write-Host "Compiling huff.exe..." -ForegroundColor Cyan
    & $CC @flags ($SOURCES + "src/main.c") -o huff.exe
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed with exit code $LASTEXITCODE"
    }
    Write-Host "Built huff.exe successfully." -ForegroundColor Green
}

function Build-UnitTests([string[]]$flags) {
    Write-Host "Compiling unit_tests.exe..." -ForegroundColor Cyan
    & $CC @flags ($SOURCES + "tests/unit_tests.c") -o unit_tests.exe
    if ($LASTEXITCODE -ne 0) {
        throw "Unit tests compilation failed with exit code $LASTEXITCODE"
    }
    Write-Host "Built unit_tests.exe successfully." -ForegroundColor Green
}

switch ($Target.ToLower()) {
    "clean" {
        Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
        Remove-Item -Force -ErrorAction SilentlyContinue *.exe, src/*.o, *.tmp*, tests/*.tmp*
        Write-Host "Clean completed." -ForegroundColor Green
    }
    "debug" {
        Build-Huff $DEBUG_FLAGS
    }
    "test" {
        Build-Huff $CFLAGS
        Build-UnitTests $CFLAGS
        Write-Host "`n--- Running C Unit Tests ---" -ForegroundColor Cyan
        .\unit_tests.exe
        Write-Host "`n--- Running Automated Integration Test Suite ---" -ForegroundColor Cyan
        python tests/test_runner.py
    }
    default {
        Build-Huff $CFLAGS
    }
}
