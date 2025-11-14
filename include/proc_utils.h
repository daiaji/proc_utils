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

PROC_UTILS_API unsigned int ProcUtils_ProcessExists(const wchar_t* process_name_or_pid);
PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, unsigned int exit_code);
PROC_UTILS_API unsigned int ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms);
PROC_UTILS_API bool ProcUtils_ProcessWaitClose(const wchar_t* process_name_or_pid, int timeout_ms);
PROC_UTILS_API bool ProcUtils_ProcessGetPath(unsigned int pid, wchar_t* out_path, int path_size);
PROC_UTILS_API unsigned int ProcUtils_Exec(const wchar_t* command, const wchar_t* working_dir, int show_mode, bool wait,
                                           const wchar_t* desktop_name);
PROC_UTILS_API unsigned int ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid);
PROC_UTILS_API bool ProcUtils_ProcessSetPriority(const wchar_t* process_name_or_pid, wchar_t priority);
PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid);
PROC_UTILS_API int ProcUtils_FindAllProcesses(const wchar_t* process_name, unsigned int* out_pids, int pids_array_size);

/**
 * @brief 【新增】获取指定进程的详细信息。
 * @param pid 进程ID。
 * @param out_info 指向 ProcUtils_ProcessInfo 结构体的指针，用于接收信息。
 * @return 成功返回 true，失败返回 false。
 */
PROC_UTILS_API bool ProcUtils_ProcessGetInfo(unsigned int pid, ProcUtils_ProcessInfo* out_info);

#ifdef __cplusplus
} // extern "C"
#endif

// -- 可选的 C++ 封装 --

#if defined(__cplusplus) && !defined(PROC_UTILS_NO_CPP_WRAPPER)
#include <optional>
#include <string>
#include <vector>

namespace ProcUtils {
class Process {
 public:
    Process(unsigned int pid = 0) : pid_(pid)
    {
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
            // Buffer was too small, resize and try again
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

    static Process exec(const std::wstring& command, const wchar_t* working_dir = nullptr, int show_mode = 1,
                        bool wait = false, const wchar_t* desktop_name = nullptr)
    {
        return Process(ProcUtils_Exec(command.c_str(), working_dir, show_mode, wait, desktop_name));
    }

    bool is_valid() const
    {
        return pid_ != 0;
    }
    unsigned int id() const
    {
        return pid_;
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
    unsigned int pid_;
};
} // namespace ProcUtils
#endif // defined(__cplusplus)