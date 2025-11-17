#pragma once

#include <stdbool.h> // For bool type in C interface
#include <wchar.h>   // For wchar_t

#if defined(_WIN32)
#include <windows.h> // For HANDLE, DWORD etc.
#if defined(PROC_UTILS_STATIC_LIB)
#define PROC_UTILS_API
#elif defined(PROC_UTILS_EXPORTS)
#define PROC_UTILS_API __declspec(dllexport)
#else
#define PROC_UTILS_API __declspec(dllimport)
#endif
#else
#define PROC_UTILS_API
#endif

// -- C 语言接口 --

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct ProcUtils_ProcessInfo
 * @brief 存储进程的详细信息 (增强版)。
 */
typedef struct {
    DWORD pid;
    DWORD parent_pid;
    DWORD session_id;
    wchar_t exe_path[260 /*MAX_PATH*/];
    wchar_t command_line[2048];            // 完整的命令行
    unsigned long long memory_usage_bytes; // 进程工作集大小
    DWORD thread_count;
} ProcUtils_ProcessInfo;

/**
 * @struct ProcUtils_ProcessResult
 * @brief 创建进程后返回的结果，包含 PID、句柄以及错误码。
 * @note 调用者获得句柄后，有责任在不再需要时调用 CloseHandle() 来释放它。
 */
typedef struct {
    DWORD pid;
    void* process_handle;  // 类型为 HANDLE
    DWORD last_error_code; // 失败时，这里会包含 GetLastError() 的值
} ProcUtils_ProcessResult;

// --- 模块 1：进程查找与枚举 ---

PROC_UTILS_API void* ProcUtils_OpenProcessByPid(DWORD pid, DWORD desired_access);
PROC_UTILS_API void* ProcUtils_OpenProcessByName(const wchar_t* process_name, DWORD desired_access);
PROC_UTILS_API int ProcUtils_FindAllProcesses(const wchar_t* process_name, DWORD* out_pids, int pids_array_size);
PROC_UTILS_API DWORD ProcUtils_ProcessExists(const wchar_t* process_name_or_pid);

// --- 模块 2：进程创建与执行 ---

PROC_UTILS_API ProcUtils_ProcessResult ProcUtils_CreateProcess(const wchar_t* command, const wchar_t* working_dir,
                                                               int show_mode, const wchar_t* desktop_name);
PROC_UTILS_API DWORD ProcUtils_LaunchProcess(const wchar_t* command, const wchar_t* working_dir, int show_mode,
                                             const wchar_t* desktop_name);
PROC_UTILS_API ProcUtils_ProcessResult ProcUtils_CreateProcessAsSystem(const wchar_t* command,
                                                                       const wchar_t* working_dir, int show_mode);

// --- 模块 3：进程信息获取 ---

PROC_UTILS_API bool ProcUtils_ProcessGetInfo(DWORD pid, ProcUtils_ProcessInfo* out_info);
PROC_UTILS_API bool ProcUtils_ProcessGetCommandLine(DWORD pid, wchar_t* out_cmd, int cmd_size);
PROC_UTILS_API bool ProcUtils_ProcessGetPath(DWORD pid, wchar_t* out_path, int path_size);
PROC_UTILS_API DWORD ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid);

// --- 模块 4：进程控制与交互 ---

PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, UINT exit_code);
PROC_UTILS_API bool ProcUtils_TerminateProcessByPid(DWORD pid, UINT exit_code);
PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid);
PROC_UTILS_API bool ProcUtils_TerminateProcessTreeByPid(DWORD pid);
PROC_UTILS_API bool ProcUtils_ProcessSetPriority(const wchar_t* process_name_or_pid, wchar_t priority);
PROC_UTILS_API DWORD ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms);
PROC_UTILS_API bool ProcUtils_ProcessWaitClose(const wchar_t* process_name_or_pid, int timeout_ms);
PROC_UTILS_API bool ProcUtils_WaitForProcessExit(void* process_handle, int timeout_ms);

#ifdef __cplusplus
} // extern "C"
#endif

// -- 可选的 C++ 封装 --

#if defined(__cplusplus) && !defined(PROC_UTILS_NO_CPP_WRAPPER)
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ProcUtils {
class Process {
 public:
    // --- 构造与生命周期 (移动唯一) ---
    Process() : handle_(NULL), pid_(0)
    {
    }

    ~Process()
    {
        if (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle_);
        }
    }

    Process(Process&& other) noexcept : handle_(other.handle_), pid_(other.pid_)
    {
        other.handle_ = NULL;
        other.pid_ = 0;
    }

    Process& operator=(Process&& other) noexcept
    {
        if (this != &other) {
            if (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE) {
                ::CloseHandle(handle_);
            }
            handle_ = other.handle_;
            pid_ = other.pid_;
            other.handle_ = NULL;
            other.pid_ = 0;
        }
        return *this;
    }

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // --- 静态工厂方法 (推荐的创建方式) ---

    static std::optional<Process> open_by_pid(DWORD pid, DWORD access = PROCESS_ALL_ACCESS)
    {
        HANDLE h = ProcUtils_OpenProcessByPid(pid, access);
        if (h) {
            return Process(h, pid);
        }
        return std::nullopt;
    }

    static std::optional<Process> open_by_name(const std::wstring& name, DWORD access = PROCESS_ALL_ACCESS)
    {
        HANDLE h = ProcUtils_OpenProcessByName(name.c_str(), access);
        if (h) {
            DWORD pid = ::GetProcessId(h);
            return Process(h, pid);
        }
        return std::nullopt;
    }

    static std::vector<DWORD> find_all_by_name(const std::wstring& name)
    {
        std::vector<DWORD> pids;
        int count = ProcUtils_FindAllProcesses(name.c_str(), nullptr, 0);
        if (count > 0) {
            pids.resize(count);
            ProcUtils_FindAllProcesses(name.c_str(), pids.data(), count);
        }
        return pids;
    }

    static std::optional<Process> exec(const std::wstring& command, const wchar_t* working_dir = nullptr,
                                       int show_mode = SW_SHOW, const wchar_t* desktop_name = nullptr)
    {
        ProcUtils_ProcessResult result = ProcUtils_CreateProcess(command.c_str(), working_dir, show_mode, desktop_name);
        if (result.pid > 0) {
            return Process(static_cast<HANDLE>(result.process_handle), result.pid);
        }
        SetLastError(result.last_error_code);
        return std::nullopt;
    }

    static std::optional<Process> exec_as_system(const std::wstring& command, const wchar_t* working_dir = nullptr,
                                                 int show_mode = SW_SHOW)
    {
        ProcUtils_ProcessResult result = ProcUtils_CreateProcessAsSystem(command.c_str(), working_dir, show_mode);
        if (result.pid > 0) {
            return Process(static_cast<HANDLE>(result.process_handle), result.pid);
        }
        SetLastError(result.last_error_code);
        return std::nullopt;
    }

    static DWORD launch(const std::wstring& command, const wchar_t* working_dir = nullptr, int show_mode = SW_SHOW,
                        const wchar_t* desktop_name = nullptr)
    {
        return ProcUtils_LaunchProcess(command.c_str(), working_dir, show_mode, desktop_name);
    }

    // --- 成员方法 ---
    bool is_valid() const
    {
        return handle_ != NULL && handle_ != INVALID_HANDLE_VALUE;
    }
    DWORD id() const
    {
        return pid_;
    }
    HANDLE handle() const
    {
        return handle_;
    }

    bool terminate(UINT exit_code = 0)
    {
        if (!is_valid())
            return false;
        return ProcUtils_TerminateProcessByPid(pid_, exit_code);
    }

    static bool terminate_by_pid(DWORD pid, UINT exit_code = 0)
    {
        return ProcUtils_TerminateProcessByPid(pid, exit_code);
    }

    bool terminate_tree()
    {
        if (!is_valid())
            return false;
        return ProcUtils_TerminateProcessTreeByPid(pid_);
    }

    bool wait_for_exit(int timeout_ms = -1)
    {
        if (!is_valid())
            return false;
        return ProcUtils_WaitForProcessExit(handle_, timeout_ms);
    }

    std::optional<ProcUtils_ProcessInfo> get_info() const
    {
        if (!is_valid())
            return std::nullopt;
        ProcUtils_ProcessInfo info;
        if (ProcUtils_ProcessGetInfo(pid_, &info)) {
            return info;
        }
        return std::nullopt;
    }

    std::optional<std::wstring> get_command_line() const
    {
        if (!is_valid())
            return std::nullopt;
        wchar_t buffer[2048];
        if (ProcUtils_ProcessGetCommandLine(pid_, buffer, _countof(buffer))) {
            return buffer;
        }
        return std::nullopt;
    }

    std::wstring get_path() const
    {
        if (!is_valid())
            return L"";
        wchar_t buffer[MAX_PATH];
        // --- 修正点 ---
        // 将 ProcUtils_GetProcessPath 改为 ProcUtils_ProcessGetPath
        if (ProcUtils_ProcessGetPath(pid_, buffer, _countof(buffer))) {
            return buffer;
        }
        return L"";
    }

 private:
    Process(HANDLE handle, DWORD pid) : handle_(handle), pid_(pid)
    {
    }

    HANDLE handle_;
    DWORD pid_;
};
} // namespace ProcUtils
#endif // defined(__cplusplus)