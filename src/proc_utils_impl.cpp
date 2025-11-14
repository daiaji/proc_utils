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
                break;
            }
        } while (Process32NextW(h_snap, &pe));
    }
    return true;
}

DWORD GetProcessPath(DWORD process_id, wchar_t* buffer, DWORD buffer_size, HANDLE existing_process_handle)
{
    *buffer = L'\0';
    ScopedHandle h_proc;
    HANDLE process_handle = existing_process_handle;

    if (!process_handle) {
        h_proc = ScopedHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process_id));
        if (!h_proc.IsValid())
            return 0;
        process_handle = h_proc;
    }

    DWORD path_length = buffer_size;
    if (QueryFullProcessImageNameW(process_handle, 0, buffer, &path_length)) {
        return path_length;
    }

    path_length = GetProcessImageFileNameW(process_handle, buffer, buffer_size);
    if (path_length) {
        wchar_t device_path[MAX_PATH];
        wchar_t drive_letter[3] = L"A:";
        DWORD drive_mask = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (!(drive_mask & (1 << i)))
                continue;
            drive_letter[0] = L'A' + i;
            if (QueryDosDeviceW(drive_letter, device_path, _countof(device_path)) > 2) {
                size_t device_path_len = wcslen(device_path);
                if (wcsncmp(device_path, buffer, device_path_len) == 0 && buffer[device_path_len] == L'\\') {
                    wchar_t temp_path[MAX_PATH];
                    wcscpy_s(temp_path, _countof(temp_path), buffer + device_path_len);
                    wcscpy_s(buffer, buffer_size, drive_letter);
                    wcscat_s(buffer, buffer_size, temp_path);
                    path_length = (DWORD)wcslen(buffer);
                    break;
                }
            }
        }
    }
    return path_length;
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
                return true;
            }
        }
        else {
            if (_wcsicmp(pe.szExeFile, process_name_or_pid) == 0) {
                found_pid = pe.th32ProcessID;
                return true;
            }
        }
        return false;
    });
    return found_pid;
}

int FindAllProcesses(const wchar_t* process_name, unsigned int* out_pids, int pids_array_size)
{
    int found_count = 0;
    bool success = ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (_wcsicmp(pe.szExeFile, process_name) == 0) {
            if (found_count < pids_array_size) {
                out_pids[found_count] = pe.th32ProcessID;
            }
            found_count++;
        }
        return false;
    });

    return success ? found_count : -1;
}

DWORD GetParentProcessId(DWORD child_pid)
{
    DWORD parent_pid = 0;
    ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (pe.th32ProcessID == child_pid) {
            parent_pid = pe.th32ParentProcessID;
            return true;
        }
        return false;
    });
    return parent_pid;
}

bool WaitForProcess(const wchar_t* process_name_or_pid, int timeout_ms, bool wait_for_close, DWORD* out_pid)
{
    const bool wait_indefinitely = timeout_ms < 0;
    const ULONGLONG start_time = GetTickCount64();

    DWORD initial_pid = FindProcess(process_name_or_pid);

    while (!g_procutils_should_exit.load(std::memory_order_relaxed)) {
        DWORD current_pid = FindProcess(process_name_or_pid);

        bool condition_met = wait_for_close ? (current_pid == 0) : (current_pid != 0);

        if (condition_met) {
            if (out_pid)
                *out_pid = current_pid;
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
                DWORD wait_result = MsgWaitForMultipleObjects(1, &raw_proc_handle, FALSE, 100, QS_ALLINPUT);
                if (wait_result == WAIT_OBJECT_0) {
                    continue;
                }
                else if (wait_result == WAIT_OBJECT_0 + 1) {
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