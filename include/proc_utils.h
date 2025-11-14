#pragma once

// --- 新增的、完整的宏定义逻辑 ---
#if defined(_WIN32)
// 如果定义了 PROC_UTILS_STATIC_LIB，则表示是静态链接，API宏为空
#if defined(PROC_UTILS_STATIC_LIB)
#define PROC_UTILS_API
// 否则，处理动态链接库的导出/导入
#elif defined(PROC_UTILS_EXPORTS)
#define PROC_UTILS_API __declspec(dllexport)
#else
#define PROC_UTILS_API __declspec(dllimport)
#endif
#else
// 非 Windows 平台，宏为空
#define PROC_UTILS_API
#endif
// --- 修改结束 ---

// 定义导出的 C 语言接口
extern "C" {

/**
 * @brief 按进程名或PID查找进程。
 * @param process_name_or_pid 进程名 (如 "explorer.exe") 或 PID 字符串 (如 "1234")。
 * @return 存在则返回第一个匹配的进程PID，否则返回0。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API unsigned int ProcUtils_ProcessExists(const wchar_t* process_name_or_pid);

/**
 * @brief 强制终止一个进程。
 * @param process_name_or_pid 进程名或PID字符串。
 * @param exit_code 进程的退出码。
 * @return 成功返回 true，失败返回 false。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, unsigned int exit_code);

/**
 * @brief 等待指定进程出现。
 * @param process_name 进程名。
 * @param timeout_ms 等待的毫秒数。-1 表示无限等待。
 * @return 进程出现则返回其PID，超时或出错返回0。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API unsigned int ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms);

/**
 * @brief 等待指定进程结束。
 * @param process_name_or_pid 进程名或PID字符串。
 * @param timeout_ms 等待的毫秒数。-1 表示无限等待。
 * @return 进程正常结束返回 true，超时或出错返回 false。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API bool ProcUtils_ProcessWaitClose(const wchar_t* process_name_or_pid, int timeout_ms);

/**
 * @brief 获取指定PID进程的完整路径。
 * @param pid 进程ID。
 * @param out_path 用于接收路径的缓冲区。
 * @param path_size 缓冲区大小 (以 wchar_t 为单位)。
 * @return 成功返回 true，失败返回 false。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API bool ProcUtils_ProcessGetPath(unsigned int pid, wchar_t* out_path, int path_size);

/**
 * @brief 执行一个外部程序。
 * @param command 完整的命令行字符串。
 * @param working_dir 程序的工作目录，可为 nullptr。
 * @param show_mode 窗口显示模式 (如 SW_HIDE, SW_SHOW)。
 * @param wait 如果为 true，则等待程序执行结束。
 * @param desktop_name 目标桌面，如 "WinSta0\\Winlogon"，nullptr 表示当前桌面。
 * @return 成功启动进程则返回其PID，失败返回0。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API unsigned int ProcUtils_Exec(const wchar_t* command, const wchar_t* working_dir, int show_mode, bool wait,
                                           const wchar_t* desktop_name);

/**
 * @brief 获取指定进程的父进程ID。
 * @param process_name_or_pid 进程名或PID字符串。
 * @return 成功返回父进程PID，失败返回0。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API unsigned int ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid);

/**
 * @brief 设置进程的优先级。
 * @param process_name_or_pid 进程名或PID字符串。
 * @param priority 优先级字符: L(ow), B(elow normal), N(ormal), A(bove normal), H(igh), R(ealtime)。
 * @return 成功返回 true，失败返回 false。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API bool ProcUtils_ProcessSetPriority(const wchar_t* process_name_or_pid, wchar_t priority);

/**
 * @brief 终止一个进程及其所有子进程。
 * @param process_name_or_pid 进程名或PID字符串。
 * @return 成功返回 true，失败返回 false。失败时可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid);

/**
 * @brief 【新增】查找所有名为 process_name 的进程。
 * @param process_name 进程名 (如 "notepad.exe")。
 * @param out_pids 用于接收 PID 的数组。
 * @param pids_array_size 数组的大小 (元素个数)。
 * @return 实际找到的进程数量。如果数量超过数组大小，则只填充数组大小个 PID，但返回值仍是实际找到的总数。
 *         如果函数出错，返回-1，可通过 GetLastError() 获取错误信息。
 */
PROC_UTILS_API int ProcUtils_FindAllProcesses(const wchar_t* process_name, unsigned int* out_pids, int pids_array_size);

} // extern "C"