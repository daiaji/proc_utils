# run_tests.ps1
# PowerShell script to run unit tests for proc_utils-ffi.
# Usage: 
#   .\run_tests.ps1             # Run tests normally
#   .\run_tests.ps1 -Profile    # Run tests with LuaJIT Profiler enabled (generates profile.log)

param (
    [switch]$Profile
)

# --- Configuration ---
$LUAJIT_EXE = "luajit.exe"
$TEST_SCRIPT = "tests/proc_utils_spec.lua"
$LUAUNIT_DIR = "vendor/luaunit"

# --- Environment Checks ---

# 1. Check for LuaJIT executable
if (-not (Get-Command $LUAJIT_EXE -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] '$LUAJIT_EXE' not found in your PATH." -ForegroundColor Red
    Write-Host "Please install LuaJIT or ensure its directory is in the PATH environment variable."
    exit 1
}

# 2. Check for Test Script
if (-not (Test-Path $TEST_SCRIPT)) {
    Write-Host "[ERROR] Test script not found: $TEST_SCRIPT" -ForegroundColor Red
    exit 1
}

# 3. Check and Initialize LuaUnit Submodule
if (-not (Test-Path "$LUAUNIT_DIR/luaunit.lua")) {
    Write-Host "[WARN] LuaUnit submodule not found at $LUAUNIT_DIR." -ForegroundColor Yellow
    Write-Host "Attempting to initialize submodule..."
    
    git submodule update --init --recursive
    
    if (-not (Test-Path "$LUAUNIT_DIR/luaunit.lua")) {
        Write-Host "[ERROR] Failed to initialize LuaUnit submodule." -ForegroundColor Red
        Write-Host "Please run: git submodule add https://github.com/bluebird75/luaunit.git vendor/luaunit"
        exit 1
    }
    Write-Host "[SUCCESS] Submodule initialized." -ForegroundColor Green
}

# --- Execution ---

$ArgsList = @()

if ($Profile) {
    Write-Host "[INFO] LuaJIT Profiler Enabled (-jp=fl,profile.log)" -ForegroundColor Magenta
    # -jp=fl: Function level profiling with Line numbers
    $ArgsList += "-jp=fl,profile.log"
}

$ArgsList += $TEST_SCRIPT

Write-Host "Running tests..." -ForegroundColor Cyan
Write-Host "Command: $LUAJIT_EXE $ArgsList" -ForegroundColor Gray
Write-Host "===================================="

# Run the command
& $LUAJIT_EXE $ArgsList

# --- Result Handling ---

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "===================================="
    Write-Host "[RESULT] Tests failed (Exit Code: $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
} else {
    Write-Host ""
    Write-Host "===================================="
    Write-Host "[RESULT] All tests passed successfully." -ForegroundColor Green
    
    if ($Profile) {
        if (Test-Path "profile.log") {
            Write-Host ""
            Write-Host "[INFO] Profiler data saved to: $(Resolve-Path profile.log)" -ForegroundColor Magenta
            Write-Host "       Use 'type profile.log' to view results."
        } else {
            Write-Host "[WARN] Profiler enabled but 'profile.log' was not generated." -ForegroundColor Yellow
        }
    }
    exit 0
}