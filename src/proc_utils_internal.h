#pragma once

// --- Preprocessor Definitions ---
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

// --- Windows System Headers (in correct order) ---
#include <windows.h>

#include <psapi.h>
#include <tlhelp32.h>

// --- C++ Standard Library Headers ---
#include <atomic>
#include <functional>
#include <string> // For std::to_wstring in C++ wrapper
#include <vector>

// --- C Standard Library Headers ---
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>

// --- Project Public Header ---
#include "proc_utils.h"

// --- RAII Wrapper ---
class ScopedHandle {
 public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle_(h)
    {
    }
    ~ScopedHandle()
    {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != NULL) {
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
            if (handle_ != INVALID_HANDLE_VALUE && handle_ != NULL)
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

// --- Global Variables & Forward Declarations ---
extern std::atomic<bool> g_procutils_should_exit;

void ProcUtils_MsgWait(int duration_ms);

namespace ProcUtils::Internal {
bool ForEachProcess(const std::function<bool(const PROCESSENTRY32W&)>& callback);
DWORD FindProcess(const wchar_t* process_name_or_pid);
int FindAllProcesses(const wchar_t* process_name, unsigned int* out_pids, int pids_array_size);
DWORD GetProcessPath(DWORD process_id, wchar_t* buffer, DWORD buffer_size, HANDLE existing_process_handle = NULL);
DWORD GetParentProcessId(DWORD child_pid);
bool WaitForProcess(const wchar_t* process_name_or_pid, int timeout_ms, bool wait_for_close, DWORD* out_pid);
bool GetProcessInfo(DWORD pid, ProcUtils_ProcessInfo* out_info);
} // namespace ProcUtils::Internal