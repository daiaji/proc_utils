#include "proc_utils_internal.h"

namespace ProcUtils::Internal {

bool ForEachProcess(const std::function<bool(const PROCESSENTRY32W&)>& callback)
{
    ScopedHandle h_snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!h_snap.IsValid()) {
        return false;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(h_snap, &pe)) {
        do {
            if (callback(pe)) {
                break; // Callback requested to stop iteration
            }
        } while (Process32NextW(h_snap, &pe));
    }
    return true;
}

DWORD GetProcessPath(DWORD process_id, wchar_t* buffer, DWORD buffer_size, HANDLE existing_process_handle)
{
    if (!buffer || buffer_size == 0)
        return 0;
    *buffer = L'\0';
    ScopedHandle h_proc;
    HANDLE process_handle = existing_process_handle;

    if (!process_handle) {
        h_proc = ScopedHandle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
        if (!h_proc.IsValid())
            return 0;
        process_handle = h_proc;
    }

    DWORD path_length = buffer_size;
    if (QueryFullProcessImageNameW(process_handle, 0, buffer, &path_length)) {
        return path_length;
    }

    // Fallback for older systems or restricted processes
    path_length = GetModuleFileNameExW(process_handle, NULL, buffer, buffer_size);
    if (path_length > 0) {
        return path_length;
    }

    return 0;
}

DWORD FindProcess(const wchar_t* process_name_or_pid)
{
    if (!process_name_or_pid || !*process_name_or_pid) {
        return 0;
    }

    wchar_t* end_ptr;
    DWORD target_pid = wcstol(process_name_or_pid, &end_ptr, 10);
    bool is_pid_search = (*end_ptr == L'\0');

    DWORD found_pid = 0;
    ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (is_pid_search) {
            if (pe.th32ProcessID == target_pid) {
                found_pid = pe.th32ProcessID;
                return true; // Stop iteration
            }
        }
        else {
            if (_wcsicmp(pe.szExeFile, process_name_or_pid) == 0) {
                found_pid = pe.th32ProcessID;
                return true; // Stop iteration
            }
        }
        return false; // Continue iteration
    });
    return found_pid;
}

// --- 修正点 ---
int FindAllProcesses(const wchar_t* process_name, DWORD* out_pids, int pids_array_size)
{
    int found_count = 0;
    int stored_count = 0;
    bool success = ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (_wcsicmp(pe.szExeFile, process_name) == 0) {
            if (out_pids && stored_count < pids_array_size) {
                out_pids[stored_count++] = pe.th32ProcessID;
            }
            found_count++;
        }
        return false; // Continue iteration
    });

    if (!success)
        return -1;

    // If buffer was provided, return number of items stored.
    // If no buffer, return total number found.
    return out_pids ? stored_count : found_count;
}

DWORD GetParentProcessId(DWORD child_pid)
{
    DWORD parent_pid = 0;
    ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (pe.th32ProcessID == child_pid) {
            parent_pid = pe.th32ParentProcessID;
            return true; // Stop iteration
        }
        return false; // Continue iteration
    });
    return parent_pid;
}

bool WaitForProcess(const wchar_t* process_name_or_pid, int timeout_ms, bool wait_for_close, DWORD* out_pid)
{
    const bool wait_indefinitely = timeout_ms < 0;
    const ULONGLONG start_time = GetTickCount64();

    DWORD initial_pid = wait_for_close ? FindProcess(process_name_or_pid) : 0;

    while (!g_procutils_should_exit.load(std::memory_order_relaxed)) {
        DWORD current_pid = FindProcess(process_name_or_pid);

        bool condition_met = wait_for_close ? (current_pid == 0) : (current_pid != 0);

        if (condition_met) {
            if (out_pid)
                *out_pid = wait_for_close ? initial_pid : current_pid;
            return true;
        }

        if (!wait_indefinitely && (GetTickCount64() - start_time >= (DWORD)timeout_ms)) {
            SetLastError(WAIT_TIMEOUT);
            break;
        }

        if (wait_for_close && initial_pid != 0) {
            ScopedHandle proc_handle(OpenProcess(SYNCHRONIZE, FALSE, initial_pid));
            if (proc_handle.IsValid()) {
                HANDLE raw_proc_handle = proc_handle;
                DWORD remaining_time =
                    wait_indefinitely ? 100 : (DWORD)timeout_ms - (DWORD)(GetTickCount64() - start_time);
                if ((int)remaining_time <= 0)
                    remaining_time = 1;

                DWORD wait_result = MsgWaitForMultipleObjects(1, &raw_proc_handle, FALSE, remaining_time, QS_ALLINPUT);

                if (wait_result == WAIT_OBJECT_0) { // Process terminated
                    continue;
                }
                if (wait_result == WAIT_OBJECT_0 + 1) { // Windows message
                    ProcUtils_MsgWait(-1);
                }
                continue;
            }
        }
        ProcUtils_MsgWait(100);
    }

    if (out_pid)
        *out_pid = 0;
    return false;
}

} // namespace ProcUtils::Internal