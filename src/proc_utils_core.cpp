#include <cstdlib>
#include <cwctype> // for towupper

#include "proc_utils.h"
#include "proc_utils_internal.h"

// 定义全局原子标志
std::atomic<bool> g_procutils_should_exit(false);

void ProcUtils_MsgWait(int duration_ms)
{
    if (g_procutils_should_exit.load(std::memory_order_relaxed))
        return;

    ULONGLONG start_time = GetTickCount64();
    DWORD wait_time = (duration_ms < 0) ? 0 : 1;

    do {
        if (MsgWaitForMultipleObjects(0, NULL, FALSE, wait_time, QS_ALLINPUT) == WAIT_OBJECT_0) {
            MSG msg;
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    g_procutils_should_exit.store(true, std::memory_order_relaxed);
                    return;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        if (duration_ms <= 0)
            break;
    } while (GetTickCount64() - start_time < (DWORD)duration_ms);
}

namespace { // 匿名命名空间，用于文件内私有函数

void TerminateProcessTree(DWORD process_id)
{
    ScopedHandle h_snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!h_snap.IsValid())
        return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(h_snap, &pe)) {
        do {
            if (pe.th32ParentProcessID == process_id) {
                TerminateProcessTree(pe.th32ProcessID); // 递归终止子进程
            }
        } while (Process32NextW(h_snap, &pe));
    }

    // 杀死所有子进程后，再杀死自身
    ScopedHandle h_proc(OpenProcess(PROCESS_TERMINATE, FALSE, process_id));
    if (h_proc.IsValid()) {
        TerminateProcess(h_proc, 1);
    }
}

} // anonymous namespace

// --- 导出的 C API 实现 ---

extern "C" {

PROC_UTILS_API unsigned int ProcUtils_ProcessExists(const wchar_t* process_name_or_pid)
{
    if (!process_name_or_pid || !*process_name_or_pid)
        return 0;
    return ProcUtils::Internal::FindProcess(process_name_or_pid);
}

PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, unsigned int exit_code)
{
    DWORD pid = ProcUtils_ProcessExists(process_name_or_pid);
    if (!pid) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }

    ScopedHandle h_process(OpenProcess(PROCESS_TERMINATE, FALSE, pid));
    if (!h_process.IsValid()) {
        return false;
    }

    return ::TerminateProcess(h_process, exit_code);
}

PROC_UTILS_API unsigned int ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms)
{
    if (!process_name || !*process_name) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    DWORD found_pid = 0;
    if (ProcUtils::Internal::WaitForProcess(process_name, timeout_ms, false, &found_pid)) {
        return found_pid;
    }
    return 0;
}

PROC_UTILS_API bool ProcUtils_ProcessWaitClose(const wchar_t* process_name_or_pid, int timeout_ms)
{
    if (!process_name_or_pid || !*process_name_or_pid) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::WaitForProcess(process_name_or_pid, timeout_ms, true, nullptr);
}

PROC_UTILS_API bool ProcUtils_ProcessGetPath(unsigned int pid, wchar_t* out_path, int path_size)
{
    if (!out_path || path_size <= 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::GetProcessPath(pid, out_path, path_size) > 0;
}

PROC_UTILS_API unsigned int ProcUtils_Exec(const wchar_t* command, const wchar_t* working_dir, int show_mode, bool wait,
                                           const wchar_t* desktop_name)
{
    if (!command || !*command) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = static_cast<WORD>(show_mode);

    if (desktop_name && *desktop_name) {
        si.lpDesktop = const_cast<LPWSTR>(desktop_name);
    }

    wchar_t* cmd_writable = _wcsdup(command);
    if (!cmd_writable) {
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }

    BOOL created = CreateProcessW(nullptr, cmd_writable, nullptr, nullptr, FALSE, 0, nullptr, working_dir, &si, &pi);
    free(cmd_writable);

    if (!created)
        return 0;

    ScopedHandle hProcess(pi.hProcess);
    ScopedHandle hThread(pi.hThread);

    if (wait) {
        while (!g_procutils_should_exit.load(std::memory_order_relaxed)) {
            HANDLE process_handle = hProcess;
            DWORD wait_result = MsgWaitForMultipleObjects(1, &process_handle, FALSE, INFINITE, QS_ALLINPUT);

            if (wait_result == WAIT_OBJECT_0) {
                break; // 进程结束
            }

            if (wait_result == WAIT_OBJECT_0 + 1) {
                MSG msg;
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        g_procutils_should_exit.store(true, std::memory_order_relaxed);
                        ::TerminateProcess(hProcess, 1);
                        break;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
            else {
                break; // 等待失败
            }
        }
    }

    return pi.dwProcessId;
}

PROC_UTILS_API unsigned int ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid)
{
    DWORD pid = ProcUtils_ProcessExists(process_name_or_pid);
    if (!pid) {
        SetLastError(ERROR_NOT_FOUND);
        return 0;
    }
    return ProcUtils::Internal::GetParentProcessId(pid);
}

PROC_UTILS_API bool ProcUtils_ProcessSetPriority(const wchar_t* process_name_or_pid, wchar_t priority)
{
    DWORD priority_class;
    switch (towupper(priority)) {
    case L'L':
        priority_class = IDLE_PRIORITY_CLASS;
        break;
    case L'B':
        priority_class = BELOW_NORMAL_PRIORITY_CLASS;
        break;
    case L'N':
        priority_class = NORMAL_PRIORITY_CLASS;
        break;
    case L'A':
        priority_class = ABOVE_NORMAL_PRIORITY_CLASS;
        break;
    case L'H':
        priority_class = HIGH_PRIORITY_CLASS;
        break;
    case L'R':
        priority_class = REALTIME_PRIORITY_CLASS;
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    DWORD pid = ProcUtils_ProcessExists(process_name_or_pid);
    if (!pid) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }

    ScopedHandle h_process(OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid));
    if (!h_process.IsValid())
        return false;

    return SetPriorityClass(h_process, priority_class);
}

PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid)
{
    DWORD pid = ProcUtils_ProcessExists(process_name_or_pid);
    if (!pid) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }

    TerminateProcessTree(pid);
    return true;
}

} // extern "C"