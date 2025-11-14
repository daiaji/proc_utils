#include "proc_utils_internal.h"

namespace ProcUtils::Internal {

bool GetProcessInfo(DWORD pid, ProcUtils_ProcessInfo* out_info)
{
    if (!out_info) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

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
            return true;
        }
        return false;
    });

    if (!process_found_in_snapshot) {
        SetLastError(ERROR_NOT_FOUND);
        return false;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(h_proc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        out_info->memory_usage_bytes = pmc.WorkingSetSize;
    }
    else {
        out_info->memory_usage_bytes = 0;
    }

    if (GetProcessPath(pid, out_info->exe_path, _countof(out_info->exe_path), h_proc) == 0) {
        return false;
    }

    out_info->pid = pid;

    return true;
}

} // namespace ProcUtils::Internal

extern "C" {

PROC_UTILS_API bool ProcUtils_ProcessGetInfo(unsigned int pid, ProcUtils_ProcessInfo* out_info)
{
    if (pid == 0 || !out_info) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    return ProcUtils::Internal::GetProcessInfo(pid, out_info);
}

} // extern "C"