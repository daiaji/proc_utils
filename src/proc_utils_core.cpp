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

namespace {
void TerminateProcessTree(DWORD process_id)
{
    ProcUtils::Internal::ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (pe.th32ParentProcessID == process_id) {
            TerminateProcessTree(pe.th32ProcessID);
        }
        return false;
    });

    ScopedHandle h_proc(OpenProcess(PROCESS_TERMINATE, FALSE, process_id));
    if (h_proc.IsValid()) {
        TerminateProcess(h_proc, 1);
    }
}
} // anonymous namespace

extern "C" {

PROC_UTILS_API unsigned int ProcUtils_ProcessExists(const wchar_t* process_name_or_pid)
{
    if (!process_name_or_pid || !*process_name_or_pid) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    return ProcUtils::Internal::FindProcess(process_name_or_pid);
}

PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, unsigned int exit_code)
{
    DWORD pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
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
    if (pid == 0 || !out_path || path_size <= 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::GetProcessPath(pid, out_path, path_size) > 0;
}

PROC_UTILS_API ProcUtils_ProcessResult ProcUtils_CreateProcess(const wchar_t* command, const wchar_t* working_dir,
                                                               int show_mode, const wchar_t* desktop_name)
{
    ProcUtils_ProcessResult result = {0, NULL};
    if (!command || !*command) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return result;
    }
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = static_cast<WORD>(show_mode);
    if (desktop_name && *desktop_name) {
        si.lpDesktop = const_cast<LPWSTR>(desktop_name);
    }

    std::vector<wchar_t> cmd_buffer(command, command + wcslen(command) + 1);

    if (!CreateProcessW(nullptr, cmd_buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, working_dir, &si, &pi)) {
        return result;
    }

    // 关闭不再需要的线程句柄
    ::CloseHandle(pi.hThread);

    // 不关闭进程句柄，将其所有权转移给调用者
    result.pid = pi.dwProcessId;
    result.process_handle = pi.hProcess;

    return result;
}

PROC_UTILS_API unsigned int ProcUtils_LaunchProcess(const wchar_t* command, const wchar_t* working_dir, int show_mode,
                                                    const wchar_t* desktop_name)
{
    ProcUtils_ProcessResult result = ProcUtils_CreateProcess(command, working_dir, show_mode, desktop_name);
    if (result.pid == 0) {
        return 0; // 创建失败
    }

    // 立即关闭句柄，只返回PID
    ::CloseHandle(result.process_handle);

    return result.pid;
}

PROC_UTILS_API unsigned int ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid)
{
    DWORD child_pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
    if (!child_pid) {
        SetLastError(ERROR_NOT_FOUND);
        return 0;
    }
    return ProcUtils::Internal::GetParentProcessId(child_pid);
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
    DWORD pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
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
    DWORD pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
    if (!pid) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    TerminateProcessTree(pid);
    return true;
}

PROC_UTILS_API int ProcUtils_FindAllProcesses(const wchar_t* process_name, unsigned int* out_pids, int pids_array_size)
{
    if (!process_name || !*process_name || !out_pids || pids_array_size < 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    return ProcUtils::Internal::FindAllProcesses(process_name, out_pids, pids_array_size);
}

} // extern "C"