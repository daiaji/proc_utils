#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <atomic>

// RAII 包装器，用于自动管理 HANDLE
class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle_(h) {}
    ~ScopedHandle() {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != NULL) {
            ::CloseHandle(handle_);
        }
    }

    // 禁止拷贝
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    // 允许移动
    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            if (handle_ != INVALID_HANDLE_VALUE && handle_ != NULL) ::CloseHandle(handle_);
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    operator HANDLE() const { return handle_; }
    bool IsValid() const { return handle_ != INVALID_HANDLE_VALUE && handle_ != NULL; }

private:
    HANDLE handle_;
}; // <--- 修正：添加了分号

// 全局退出标志，改为原子类型以支持多线程
extern std::atomic<bool> g_procutils_should_exit;

// 极简的非阻塞等待函数
void ProcUtils_MsgWait(int duration_ms);


// 内部实现函数的命名空间
namespace ProcUtils::Internal
{
    DWORD FindProcess(const wchar_t* process_name_or_pid);

    DWORD GetProcessPath(DWORD process_id, wchar_t* buffer, DWORD buffer_size);

    DWORD GetParentProcessId(DWORD child_pid);

    bool WaitForProcess(const wchar_t* process_name_or_pid, int timeout_ms, bool wait_for_close, DWORD* out_pid);

} // namespace ProcUtils::Internal