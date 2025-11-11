#pragma once

#ifdef PROC_UTILS_EXPORTS
#define PROC_UTILS_API __declspec(dllexport)
#else
#define PROC_UTILS_API __declspec(dllimport)
#endif

// 定义导出的 C 语言接口
extern "C" {

    /**
     * @brief 按进程名或PID查找进程。
     * @param process_name_or_pid 进程名 (如 "explorer.exe") 或 PID 字符串 (如 "1234")。
     * @return 存在则返回进程PID，否则返回0。
     */
    PROC_UTILS_API unsigned int ProcUtils_ProcessExists(const wchar_t* process_name_or_pid);

    /**
     * @brief 强制终止一个进程。
     * @param process_name_or_pid 进程名或PID字符串。
     * @param exit_code 进程的退出码。
     * @return 成功返回 true，失败返回 false。
     */
    PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, unsigned int exit_code);

    /**
     * @brief 等待指定进程出现。
     * @param process_name 进程名。
     * @param timeout_ms 等待的毫秒数。-1 表示无限等待。
     * @return 进程出现则返回其PID，超时或出错返回0。
     */
    PROC_UTILS_API unsigned int ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms);

    /**
     * @brief 等待指定进程结束。
     * @param process_name_or_pid 进程名或PID字符串。
     * @param timeout_ms 等待的毫秒数。-1 表示无限等待。
     * @return 进程正常结束返回 true，超时或出错返回 false。
     */
    PROC_UTILS_API bool ProcUtils_ProcessWaitClose(const wchar_t* process_name_or_pid, int timeout_ms);

    /**
     * @brief 获取指定PID进程的完整路径。
     * @param pid 进程ID。
     * @param out_path 用于接收路径的缓冲区。
     * @param path_size 缓冲区大小 (以 wchar_t 为单位)。
     * @return 成功返回 true，失败返回 false。
     */
    PROC_UTILS_API bool ProcUtils_ProcessGetPath(unsigned int pid, wchar_t* out_path, int path_size);

    /**
     * @brief 执行一个外部程序。
     * @param command 完整的命令行字符串。
     * @param working_dir 程序的工作目录，可为 nullptr。
     * @param show_mode 窗口显示模式 (如 SW_HIDE, SW_SHOW)。
     * @param wait 如果为 true，则等待程序执行结束。
     * @param desktop_name 目标桌面，如 "WinSta0\\Winlogon"，nullptr 表示当前桌面。
     * @return 成功启动进程则返回其PID，失败返回0。
     */
    PROC_UTILS_API unsigned int ProcUtils_Exec(const wchar_t* command, const wchar_t* working_dir, int show_mode, bool wait, const wchar_t* desktop_name);

    /**
     * @brief 获取指定进程的父进程ID。
     * @param process_name_or_pid 进程名或PID字符串。
     * @return 成功返回父进程PID，失败返回0。
     */
    PROC_UTILS_API unsigned int ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid);

    /**
     * @brief 设置进程的优先级。
     * @param process_name_or_pid 进程名或PID字符串。
     * @param priority 优先级字符: L(ow), B(elow normal), N(ormal), A(bove normal), H(igh), R(ealtime)。
     * @return 成功返回 true，失败返回 false。
     */
    PROC_UTILS_API bool ProcUtils_ProcessSetPriority(const wchar_t* process_name_or_pid, wchar_t priority);

    /**
     * @brief 终止一个进程及其所有子进程。
     * @param process_name_or_pid 进程名或PID字符串。
     * @return 成功返回 true，失败返回 false。
     */
    PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid);

} // extern "C"