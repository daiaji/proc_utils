#include "proc_utils_internal.h"

// --- 修正点: 将 PROCESSINFOClass 改为 PROCESSINFOCLASS ---
typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass,
                                                      PVOID ProcessInformation, ULONG ProcessInformationLength,
                                                      PULONG ReturnLength);

namespace ProcUtils::Internal {

bool GetProcessCommandLine(DWORD pid, wchar_t* buffer, int buffer_size)
{
    *buffer = L'\0';
    ScopedHandle h_proc(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!h_proc.IsValid()) {
        return false;
    }

    PROCESS_BASIC_INFORMATION pbi;
    ULONG return_len;

    HMODULE h_ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!h_ntdll) {
        return false;
    }

    pfnNtQueryInformationProcess pNtQueryInformationProcess =
        (pfnNtQueryInformationProcess)GetProcAddress(h_ntdll, "NtQueryInformationProcess");

    if (!pNtQueryInformationProcess) {
        return false;
    }

    NTSTATUS status = pNtQueryInformationProcess(h_proc, ProcessBasicInformation, &pbi, sizeof(pbi), &return_len);
    if (status != 0) {
        return false;
    }

    PEB peb;
    if (!ReadProcessMemory(h_proc, pbi.PebBaseAddress, &peb, sizeof(peb), NULL)) {
        return false;
    }

    RTL_USER_PROCESS_PARAMETERS params;
    if (!ReadProcessMemory(h_proc, peb.ProcessParameters, &params, sizeof(params), NULL)) {
        return false;
    }

    if (params.CommandLine.Length > 0) {
        SIZE_T bytes_to_read = (params.CommandLine.Length < (buffer_size - 1) * sizeof(wchar_t))
                                   ? params.CommandLine.Length
                                   : (buffer_size - 1) * sizeof(wchar_t);
        if (ReadProcessMemory(h_proc, params.CommandLine.Buffer, buffer, bytes_to_read, NULL)) {
            buffer[bytes_to_read / sizeof(wchar_t)] = L'\0';
            return true;
        }
    }

    return false;
}

bool GetProcessInfo(DWORD pid, ProcUtils_ProcessInfo* out_info)
{
    if (!out_info) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    // Zero out the struct to prevent returning garbage data on partial failure
    memset(out_info, 0, sizeof(ProcUtils_ProcessInfo));

    ScopedHandle h_proc(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!h_proc.IsValid()) {
        return false;
    }

    bool process_found_in_snapshot = false;
    ForEachProcess([&](const PROCESSENTRY32W& pe) {
        if (pe.th32ProcessID == pid) {
            out_info->parent_pid = pe.th32ParentProcessID;
            out_info->thread_count = pe.cntThreads;
            process_found_in_snapshot = true;
            return true; // Stop iteration
        }
        return false; // Continue iteration
    });

    if (!process_found_in_snapshot) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }

    if (!ProcessIdToSessionId(pid, &out_info->session_id)) {
        out_info->session_id = (DWORD)-1; // Indicate failure
    }

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(h_proc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        out_info->memory_usage_bytes = pmc.WorkingSetSize;
    }

    if (GetProcessPath(pid, out_info->exe_path, _countof(out_info->exe_path), h_proc) == 0) {
        // This is not a fatal error, path might be inaccessible
        wcscpy_s(out_info->exe_path, L"");
    }

    GetProcessCommandLine(pid, out_info->command_line, _countof(out_info->command_line));

    out_info->pid = pid;

    return true;
}

} // namespace ProcUtils::Internal

extern "C" {

PROC_UTILS_API bool ProcUtils_ProcessGetInfo(DWORD pid, ProcUtils_ProcessInfo* out_info)
{
    if (pid == 0 || !out_info) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::GetProcessInfo(pid, out_info);
}

PROC_UTILS_API bool ProcUtils_ProcessGetCommandLine(DWORD pid, wchar_t* out_cmd, int cmd_size)
{
    if (pid == 0 || !out_cmd || cmd_size <= 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::GetProcessCommandLine(pid, out_cmd, cmd_size);
}

} // extern "C"