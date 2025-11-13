#include <cstdlib>
#include <cstring> // For _wcsicmp on MSVC
#include <cwchar>  // For wcstol

#include "proc_utils_internal.h"

namespace ProcUtils::Internal {
DWORD GetProcessPath(DWORD process_id, wchar_t* buffer, DWORD buffer_size)
{
    *buffer = L'\0';
    ScopedHandle h_proc(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
    if (!h_proc.IsValid())
        return 0;

    DWORD path_length = GetProcessImageFileNameW(h_proc, buffer, buffer_size);
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
    // 修正: 增加空指针和空字符串检查，防止 _wcsicmp 崩溃
    if (!process_name_or_pid || !*process_name_or_pid) {
        return 0;
    }

    DWORD pid = 0;
    wchar_t* end_ptr;
    pid = wcstol(process_name_or_pid, &end_ptr, 10);
    if (*end_ptr != L'\0') { // 包含非数字字符，视为进程名
        pid = 0;
    }

    ScopedHandle h_snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!h_snap.IsValid())
        return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(h_snap, &pe)) {
        do {
            if (pid) { // 按PID查找
                if (pe.th32ProcessID == pid) {
                    return pid;
                }
            }
            else { // 按名称查找
                if (_wcsicmp(pe.szExeFile, process_name_or_pid) == 0) {
                    return pe.th32ProcessID;
                }
            }
        } while (Process32NextW(h_snap, &pe));
    }
    return 0; // 未找到
}

DWORD GetParentProcessId(DWORD child_pid)
{
    ScopedHandle h_snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!h_snap.IsValid())
        return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(h_snap, &pe)) {
        do {
            if (pe.th32ProcessID == child_pid) {
                return pe.th32ParentProcessID;
            }
        } while (Process32NextW(h_snap, &pe));
    }
    return 0;
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
                if (wait_result == WAIT_OBJECT_0) { // 进程结束
                    // 循环将确认进程确实已消失
                    continue;
                }
                else if (wait_result == WAIT_OBJECT_0 + 1) { // 消息
                    ProcUtils_MsgWait(-1);                   // 处理所有消息
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