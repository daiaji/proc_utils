#pragma once

#include <stdbool.h> // For bool type in C interface

#if defined(_WIN32)
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
 * @brief 存储进程的详细信息。
 */
typedef struct {
    unsigned int pid;
    unsigned int parent_pid;
    wchar_t exe_path[260 /*MAX_PATH*/];
    unsigned long long memory_usage_bytes; // 进程工作集大小
    unsigned int thread_count;
} ProcUtils_ProcessInfo;

/**
 * @struct ProcUtils_ProcessResult
 * @brief 创建进程后返回的结果，包含 PID 和一个有效的进程句柄。
 * @note 调用者获得句柄后，有责任在不再需要时调用 CloseHandle() 来释放它。
 */
typedef struct {
    unsigned int pid;
    void* process_handle; // 类型为 HANDLE
} ProcUtils_ProcessResult;

PROC_UTILS_API unsigned int ProcUtils_ProcessExists(const wchar_t* process_name_or_pid);
PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, unsigned int exit_code);
PROC_UTILS_API unsigned int ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms);
PROC_UTILS_API bool ProcUtils_ProcessWaitClose(const wchar_t* process_name_or_pid, int timeout_ms);
PROC_UTILS_API bool ProcUtils_ProcessGetPath(unsigned int pid, wchar_t* out_path, int path_size);
PROC_UTILS_API unsigned int ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid);
PROC_UTILS_API bool ProcUtils_ProcessSetPriority(const wchar_t* process_name_or_pid, wchar_t priority);
PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid);
PROC_UTILS_API int ProcUtils_FindAllProcesses(const wchar_t* process_name, unsigned int* out_pids, int pids_array_size);
PROC_UTILS_API bool ProcUtils_ProcessGetInfo(unsigned int pid, ProcUtils_ProcessInfo* out_info);

/**
 * @brief 【新增】创建一个新进程，并原子性地返回其 PID 和进程句柄。
 * @param command 命令行。
 * @param working_dir 工作目录，可为 NULL。
 * @param show_mode 窗口显示模式 (例如 SW_HIDE, SW_SHOW)。
 * @param desktop_name 桌面名称，可为 NULL。
 * @return 成功则返回包含有效 PID 和句柄的 ProcUtils_ProcessResult 结构体。失败则返回 {0, NULL}。
 * @warning 调用者必须负责对返回的 process_handle 调用 CloseHandle()。
 */
PROC_UTILS_API ProcUtils_ProcessResult ProcUtils_CreateProcess(const wchar_t* command, const wchar_t* working_dir,
                                                               int show_mode, const wchar_t* desktop_name);

/**
 * @brief 【新增】以“发后不理”的方式启动一个新进程，只返回 PID。
 * @param command 命令行。
 * @param working_dir 工作目录，可为 NULL。
 * @param show_mode 窗口显示模式 (例如 SW_HIDE, SW_SHOW)。
 * @param desktop_name 桌面名称，可为 NULL。
 * @return 成功则返回新进程的 PID，失败返回 0。
 */
PROC_UTILS_API unsigned int ProcUtils_LaunchProcess(const wchar_t* command, const wchar_t* working_dir, int show_mode,
                                                    const wchar_t* desktop_name);

#ifdef __cplusplus
} // extern "C"
#endif

// -- 可选的 C++ 封装 --

#if defined(__cplusplus) && !defined(PROC_UTILS_NO_CPP_WRAPPER)
#include <optional>
#include <string>
#include <utility> // For std::move
#include <vector>

// 在 C++ 封装中需要 Windows.h 的定义
#if defined(_WIN32)
#include <windows.h>
#endif

namespace ProcUtils {
class Process {
 public:
    // 默认构造
    Process() : pid_(0), handle_(NULL)
    {
    }

    // 通过 PID 构造 (不获取句柄)
    explicit Process(unsigned int pid) : pid_(pid), handle_(NULL)
    {
    }

    // 析构函数，实现 RAII
    ~Process()
    {
        if (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle_);
        }
    }

    // 删除拷贝构造和拷贝赋值
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // 实现移动构造函数
    Process(Process&& other) noexcept : pid_(other.pid_), handle_(other.handle_)
    {
        other.pid_ = 0;
        other.handle_ = NULL;
    }

    // 实现移动赋值运算符
    Process& operator=(Process&& other) noexcept
    {
        if (this != &other) {
            if (handle_ != NULL && handle_ != INVALID_HANDLE_VALUE) {
                ::CloseHandle(handle_);
            }
            pid_ = other.pid_;
            handle_ = other.handle_;
            other.pid_ = 0;
            other.handle_ = NULL;
        }
        return *this;
    }

    static Process find(const std::wstring& name_or_pid)
    {
        return Process(ProcUtils_ProcessExists(name_or_pid.c_str()));
    }

    static std::vector<Process> find_all(const std::wstring& name)
    {
        std::vector<Process> processes;
        unsigned int pids[128];
        int count = ProcUtils_FindAllProcesses(name.c_str(), pids, _countof(pids));
        if (count > _countof(pids)) {
            std::vector<unsigned int> large_pids(count);
            ProcUtils_FindAllProcesses(name.c_str(), large_pids.data(), count);
            for (int i = 0; i < count; ++i) {
                processes.emplace_back(large_pids[i]);
            }
        }
        else if (count > 0) {
            for (int i = 0; i < count; ++i) {
                processes.emplace_back(pids[i]);
            }
        }
        return processes;
    }

    /**
     * @brief 创建一个新进程，并返回一个管理其句柄的 Process 对象。
     */
    static Process exec(const std::wstring& command, const wchar_t* working_dir = nullptr, int show_mode = 1,
                        const wchar_t* desktop_name = nullptr)
    {
        ProcUtils_ProcessResult result = ProcUtils_CreateProcess(command.c_str(), working_dir, show_mode, desktop_name);
        return Process(result.pid, static_cast<HANDLE>(result.process_handle));
    }

    /**
     * @brief 以“发后不理”的方式启动一个新进程，只返回 PID。
     */
    static unsigned int launch(const std::wstring& command, const wchar_t* working_dir = nullptr, int show_mode = 1,
                               const wchar_t* desktop_name = nullptr)
    {
        return ProcUtils_LaunchProcess(command.c_str(), working_dir, show_mode, desktop_name);
    }

    bool is_valid() const
    {
        return pid_ != 0;
    }
    unsigned int id() const
    {
        return pid_;
    }
    HANDLE handle() const
    {
        return handle_;
    }

    bool close(unsigned int exit_code = 0)
    {
        if (!is_valid())
            return false;
        return ProcUtils_ProcessClose(std::to_wstring(pid_).c_str(), exit_code);
    }

    std::wstring get_path() const
    {
        if (!is_valid())
            return L"";
        wchar_t buffer[260];
        if (ProcUtils_ProcessGetPath(pid_, buffer, _countof(buffer))) {
            return buffer;
        }
        return L"";
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

 private:
    // 私有构造函数，用于 exec
    Process(unsigned int pid, HANDLE handle) : pid_(pid), handle_(handle)
    {
    }

    unsigned int pid_;
    HANDLE handle_; // C++ 封装现在持有句柄
};
} // namespace ProcUtils
#endif // defined(__cplusplus)