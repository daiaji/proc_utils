#pragma once

// --- Preprocessor Definitions ---
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

// --- Windows System Headers (in correct order) ---
#include <windows.h>

#include <psapi.h>
#include <tlhelp32.h>
#include <userenv.h>  // For CreateEnvironmentBlock
#include <winternl.h> // For PEB, RTL_USER_PROCESS_PARAMETERS
#include <wtsapi32.h>

// --- C++ Standard Library Headers ---
#include <atomic>
#include <functional>
#include <string>
#include <vector>

// --- C Standard Library Headers ---
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>

// --- Project Public Header ---
#include "proc_utils.h"

// --- Link Libraries ---
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")

// --- RAII Wrappers ---
class ScopedHandle {
 public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle_(h)
    {
    }
    ~ScopedHandle()
    {
        if (IsValid()) {
            ::CloseHandle(handle_);
        }
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_)
    {
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
        if (this != &other) {
            if (IsValid())
                ::CloseHandle(handle_);
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    operator HANDLE() const
    {
        return handle_;
    }
    bool IsValid() const
    {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != NULL;
    }

 private:
    HANDLE handle_;
};

class ScopedEnvironmentBlock {
 public:
    explicit ScopedEnvironmentBlock(void* block = nullptr) : block_(block)
    {
    }
    ~ScopedEnvironmentBlock()
    {
        if (block_) {
            DestroyEnvironmentBlock(block_);
        }
    }
    operator void*() const
    {
        return block_;
    }

 private:
    void* block_;
};

// --- Global Variables & Forward Declarations ---
extern std::atomic<bool> g_procutils_should_exit;

void ProcUtils_MsgWait(int duration_ms);

// --- Internal Function Prototypes ---
namespace ProcUtils::Internal {
bool ForEachProcess(const std::function<bool(const PROCESSENTRY32W&)>& callback);
DWORD FindProcess(const wchar_t* process_name_or_pid);
// --- 修正点 ---
int FindAllProcesses(const wchar_t* process_name, DWORD* out_pids, int pids_array_size);
DWORD GetProcessPath(DWORD process_id, wchar_t* buffer, DWORD buffer_size, HANDLE existing_process_handle = NULL);
DWORD GetParentProcessId(DWORD child_pid);
bool WaitForProcess(const wchar_t* process_name_or_pid, int timeout_ms, bool wait_for_close, DWORD* out_pid);
bool GetProcessInfo(DWORD pid, ProcUtils_ProcessInfo* out_info);
bool GetProcessCommandLine(DWORD pid, wchar_t* buffer, int buffer_size);
void TerminateProcessTree(DWORD process_id);
} // namespace ProcUtils::Internal