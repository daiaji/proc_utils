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

namespace ProcUtils::Internal {
void TerminateProcessTree(DWORD process_id)
{
    ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (pe.th32ParentProcessID == process_id) {
            TerminateProcessTree(pe.th32ProcessID);
        }
        return false; // Continue iteration
    });

    ScopedHandle h_proc(OpenProcess(PROCESS_TERMINATE, FALSE, process_id));
    if (h_proc.IsValid()) {
        ::TerminateProcess(h_proc, 1);
    }
}
} // namespace ProcUtils::Internal

extern "C" {

// --- 模块 1：进程查找与枚举 ---

PROC_UTILS_API void* ProcUtils_OpenProcessByPid(DWORD pid, DWORD desired_access)
{
    if (pid == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    HANDLE h_proc = OpenProcess(desired_access, FALSE, pid);
    return h_proc;
}

PROC_UTILS_API void* ProcUtils_OpenProcessByName(const wchar_t* process_name, DWORD desired_access)
{
    DWORD pid = ProcUtils::Internal::FindProcess(process_name);
    if (pid == 0) {
        SetLastError(ERROR_NOT_FOUND);
        return NULL;
    }
    return ProcUtils_OpenProcessByPid(pid, desired_access);
}

PROC_UTILS_API int ProcUtils_FindAllProcesses(const wchar_t* process_name, DWORD* out_pids, int pids_array_size)
{
    if (!process_name || !*process_name) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    if (!out_pids && pids_array_size > 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    return ProcUtils::Internal::FindAllProcesses(process_name, out_pids, pids_array_size);
}

PROC_UTILS_API DWORD ProcUtils_ProcessExists(const wchar_t* process_name_or_pid)
{
    if (!process_name_or_pid || !*process_name_or_pid) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    return ProcUtils::Internal::FindProcess(process_name_or_pid);
}

// --- 模块 2：进程创建与执行 ---

PROC_UTILS_API ProcUtils_ProcessResult ProcUtils_CreateProcess(const wchar_t* command, const wchar_t* working_dir,
                                                               int show_mode, const wchar_t* desktop_name)
{
    ProcUtils_ProcessResult result = {0, NULL, 0}; // 初始化错误码为 0
    if (!command || !*command) {
        result.last_error_code = ERROR_INVALID_PARAMETER;
        SetLastError(result.last_error_code); // 兼容旧行为
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
        result.last_error_code = GetLastError(); // 将错误码存入结构体
        return result;
    }

    ::CloseHandle(pi.hThread);
    result.pid = pi.dwProcessId;
    result.process_handle = pi.hProcess;
    result.last_error_code = 0; // 成功
    return result;
}

PROC_UTILS_API DWORD ProcUtils_LaunchProcess(const wchar_t* command, const wchar_t* working_dir, int show_mode,
                                             const wchar_t* desktop_name)
{
    ProcUtils_ProcessResult result = ProcUtils_CreateProcess(command, working_dir, show_mode, desktop_name);
    if (result.pid == 0) {
        SetLastError(result.last_error_code); // 维持 LaunchProcess 的旧行为
        return 0;
    }
    ::CloseHandle(result.process_handle);
    return result.pid;
}

PROC_UTILS_API ProcUtils_ProcessResult ProcUtils_CreateProcessAsSystem(const wchar_t* command,
                                                                       const wchar_t* working_dir, int show_mode)
{
    ProcUtils_ProcessResult result = {0, NULL, 0};
    DWORD last_error = 0;

    do {
        if (!command || !*command) {
            last_error = ERROR_INVALID_PARAMETER;
            break;
        }

        DWORD session_id = WTSGetActiveConsoleSessionId();
        if (session_id == 0xFFFFFFFF) {
            last_error = GetLastError();
            if (last_error == 0)
                last_error = ERROR_NO_TOKEN;
            break;
        }

        HANDLE h_user_token = NULL;
        if (!WTSQueryUserToken(session_id, &h_user_token)) {
            last_error = GetLastError();
            if (last_error == 0)
                last_error = ERROR_NO_TOKEN;
            break;
        }
        ScopedHandle scoped_user_token(h_user_token);

        HANDLE h_primary_token = NULL;
        if (!DuplicateTokenEx(scoped_user_token, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenPrimary,
                              &h_primary_token)) {
            last_error = GetLastError();
            if (last_error == 0)
                last_error = ERROR_ACCESS_DENIED;
            break;
        }
        ScopedHandle scoped_primary_token(h_primary_token);

        void* env_block = nullptr;
        if (!CreateEnvironmentBlock(&env_block, scoped_primary_token, FALSE)) {
            last_error = GetLastError();
            if (last_error == 0)
                last_error = ERROR_GEN_FAILURE;
            break;
        }
        ScopedEnvironmentBlock scoped_env(env_block);

        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = static_cast<WORD>(show_mode);
        si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");

        std::vector<wchar_t> cmd_buffer(command, command + wcslen(command) + 1);

        if (!CreateProcessAsUserW(scoped_primary_token, NULL, cmd_buffer.data(), NULL, NULL, FALSE,
                                  CREATE_UNICODE_ENVIRONMENT, env_block, working_dir, &si, &pi)) {
            last_error = GetLastError();
            if (last_error == 0)
                last_error = ERROR_GEN_FAILURE;
            break;
        }

        ::CloseHandle(pi.hThread);
        result.pid = pi.dwProcessId;
        result.process_handle = pi.hProcess;
        last_error = 0;

    } while (false);

    result.last_error_code = last_error;
    if (last_error != 0) {
        SetLastError(last_error);
    }

    return result;
}

// --- 模块 3：进程信息获取 (部分) ---

PROC_UTILS_API bool ProcUtils_ProcessGetPath(DWORD pid, wchar_t* out_path, int path_size)
{
    if (pid == 0 || !out_path || path_size <= 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::GetProcessPath(pid, out_path, path_size, NULL) > 0;
}

PROC_UTILS_API DWORD ProcUtils_ProcessGetParent(const wchar_t* process_name_or_pid)
{
    DWORD child_pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
    if (!child_pid) {
        SetLastError(ERROR_NOT_FOUND);
        return 0;
    }
    return ProcUtils::Internal::GetParentProcessId(child_pid);
}

// --- 模块 4：进程控制与交互 ---

PROC_UTILS_API bool ProcUtils_TerminateProcessByPid(DWORD pid, UINT exit_code)
{
    if (pid == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    ScopedHandle h_process(OpenProcess(PROCESS_TERMINATE, FALSE, pid));
    if (!h_process.IsValid()) {
        return false;
    }
    return ::TerminateProcess(h_process, exit_code);
}

PROC_UTILS_API bool ProcUtils_ProcessClose(const wchar_t* process_name_or_pid, UINT exit_code)
{
    DWORD pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
    if (pid == 0) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return ProcUtils_TerminateProcessByPid(pid, exit_code);
}

PROC_UTILS_API bool ProcUtils_TerminateProcessTreeByPid(DWORD pid)
{
    if (pid == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    ProcUtils::Internal::TerminateProcessTree(pid);
    return true;
}

PROC_UTILS_API bool ProcUtils_ProcessCloseTree(const wchar_t* process_name_or_pid)
{
    DWORD pid = ProcUtils::Internal::FindProcess(process_name_or_pid);
    if (!pid) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    ProcUtils_TerminateProcessTreeByPid(pid);
    return true;
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

PROC_UTILS_API DWORD ProcUtils_ProcessWait(const wchar_t* process_name, int timeout_ms)
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

PROC_UTILS_API bool ProcUtils_WaitForProcessExit(void* process_handle, int timeout_ms)
{
    if (process_handle == NULL || process_handle == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    DWORD result = WaitForSingleObject(static_cast<HANDLE>(process_handle), timeout_ms < 0 ? INFINITE : timeout_ms);
    if (result == WAIT_OBJECT_0) {
        return true;
    }
    if (result == WAIT_TIMEOUT) {
        SetLastError(WAIT_TIMEOUT);
    }
    return false;
}

} // extern "C"