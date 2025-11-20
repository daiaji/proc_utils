-- proc_utils_ffi.lua (v3.1.0 - Pure OOP Refactor with Feature Completion)
-- A pure LuaJIT FFI implementation of the proc_utils library for Windows,
-- refactored with a pure and idiomatic Lua OOP interface.
-- Author: Gemini AI
-- Version: 3.1.0

local ffi = require("ffi")

if ffi.os ~= "Windows" then
    return {
        _VERSION = "3.1.0",
        _OS_SUPPORT = false,
        error = "proc_utils-ffi only supports Windows"
    }
end

ffi.cdef[[
    /* --- CDEFS remain unchanged --- */
    /* --- Basic Types --- */
    typedef unsigned long DWORD;
    typedef long LONG;
    typedef void* HANDLE;
    typedef void* PVOID;
    typedef int BOOL;
    typedef unsigned int UINT;
    typedef wchar_t WCHAR;
    typedef WCHAR* LPWSTR;
    typedef const WCHAR* LPCWSTR;
    typedef unsigned long ULONG;
    typedef unsigned short WORD;
    typedef unsigned char BYTE;
    typedef unsigned long long ULONGLONG;

    /* --- WinAPI Constants --- */
    static const UINT CP_UTF8 = 65001;
    static const DWORD TH32CS_SNAPPROCESS = 0x00000002;
    static const DWORD PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;
    static const DWORD PROCESS_QUERY_INFORMATION = 0x0400;
    static const DWORD PROCESS_VM_READ = 0x0010;
    static const DWORD PROCESS_TERMINATE = 0x0001;
    static const DWORD PROCESS_SET_INFORMATION = 0x0200;
    static const DWORD PROCESS_ALL_ACCESS = 0x1F0FFF;
    static const DWORD SYNCHRONIZE = 0x00100000;
    static const DWORD STARTF_USESHOWWINDOW = 0x00000001;
    static const DWORD INFINITE = 0xFFFFFFFF;
    static const DWORD WAIT_TIMEOUT = 258;
    static const DWORD WAIT_OBJECT_0 = 0;
    static const DWORD ERROR_INVALID_PARAMETER = 87;
    static const DWORD ERROR_NOT_FOUND = 1168;
    static const DWORD MAXIMUM_ALLOWED = 0x02000000;
    static const DWORD CREATE_UNICODE_ENVIRONMENT = 0x00000400;
    
    /* Priority classes */
    static const DWORD IDLE_PRIORITY_CLASS = 0x00000040;
    static const DWORD BELOW_NORMAL_PRIORITY_CLASS = 0x00004000;
    static const DWORD NORMAL_PRIORITY_CLASS = 0x00000020;
    static const DWORD ABOVE_NORMAL_PRIORITY_CLASS = 0x00008000;
    static const DWORD HIGH_PRIORITY_CLASS = 0x00000080;
    static const DWORD REALTIME_PRIORITY_CLASS = 0x00000100;

    /* FormatMessage constants */
    static const DWORD FORMAT_MESSAGE_ALLOCATE_BUFFER = 0x00100;
    static const DWORD FORMAT_MESSAGE_FROM_SYSTEM = 0x001000;
    static const DWORD FORMAT_MESSAGE_IGNORE_INSERTS = 0x000200;

    /* --- Core Structures --- */
    typedef struct _PROCESSENTRY32W {
        DWORD dwSize;
        DWORD cntUsage;
        DWORD th32ProcessID;
        uintptr_t th32DefaultHeapID;
        DWORD th32ModuleID;
        DWORD cntThreads;
        DWORD th32ParentProcessID;
        LONG pcPriClassBase;
        DWORD dwFlags;
        WCHAR szExeFile[260];
    } PROCESSENTRY32W;

    typedef struct _PROCESS_INFORMATION {
        HANDLE hProcess;
        HANDLE hThread;
        DWORD dwProcessId;
        DWORD dwThreadId;
    } PROCESS_INFORMATION;

    typedef struct _STARTUPINFOW {
        DWORD cb;
        LPWSTR lpReserved;
        LPWSTR lpDesktop;
        LPWSTR lpTitle;
        DWORD dwX;
        DWORD dwY;
        DWORD dwXSize;
        DWORD dwYSize;
        DWORD dwXCountChars;
        DWORD dwYCountChars;
        DWORD dwFillAttribute;
        DWORD dwFlags;
        WORD wShowWindow;
        WORD cbReserved2;
        BYTE* lpReserved2;
        HANDLE hStdInput;
        HANDLE hStdOutput;
        HANDLE hStdError;
    } STARTUPINFOW;

    typedef enum _SECURITY_IMPERSONATION_LEVEL {
      SecurityAnonymous,
      SecurityIdentification,
      SecurityImpersonation,
      SecurityDelegation
    } SECURITY_IMPERSONATION_LEVEL;

    typedef enum _TOKEN_TYPE {
      TokenPrimary = 1,
      TokenImpersonation
    } TOKEN_TYPE;

    typedef struct {
        DWORD pid;
        DWORD parent_pid;
        DWORD session_id;
        WCHAR exe_path[260];
        WCHAR command_line[2048];
        unsigned long long memory_usage_bytes;
        DWORD thread_count;
    } ProcUtils_ProcessInfo;

    typedef struct {
        DWORD pid;
        HANDLE process_handle;
        DWORD last_error_code;
    } ProcUtils_ProcessResult;

    typedef struct _UNICODE_STRING {
        WORD Length;
        WORD MaximumLength;
        LPWSTR Buffer;
    } UNICODE_STRING;

    typedef struct _RTL_USER_PROCESS_PARAMETERS {
        BYTE Reserved1[16];
        PVOID Reserved2[10];
        UNICODE_STRING ImagePathName;
        UNICODE_STRING CommandLine;
    } RTL_USER_PROCESS_PARAMETERS;

    typedef struct _PEB {
        BYTE Reserved1[2];
        BYTE BeingDebugged;
        BYTE Reserved2[1];
        PVOID Reserved3[2];
        PVOID Ldr;
        RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
    } PEB;

    typedef struct _PROCESS_BASIC_INFORMATION {
        intptr_t ExitStatus;
        PEB* PebBaseAddress;
        uintptr_t AffinityMask;
        int32_t BasePriority;
        uintptr_t UniqueProcessId;
        uintptr_t InheritedFromUniqueProcessId;
    } PROCESS_BASIC_INFORMATION;
    
    typedef struct _PROCESS_MEMORY_COUNTERS_EX {
      DWORD cb;
      DWORD PageFaultCount;
      size_t PeakWorkingSetSize;
      size_t WorkingSetSize;
      size_t QuotaPeakPagedPoolUsage;
      size_t QuotaPagedPoolUsage;
      size_t QuotaPeakNonPagedPoolUsage;
      size_t QuotaNonPagedPoolUsage;
      size_t PagefileUsage;
      size_t PeakPagefileUsage;
      size_t PrivateUsage;
    } PROCESS_MEMORY_COUNTERS_EX;

    typedef enum _PROCESSINFOCLASS {
        ProcessBasicInformation = 0
    } PROCESSINFOCLASS;

    /* --- Function Prototypes --- */
    HANDLE CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID);
    BOOL Process32FirstW(HANDLE hSnapshot, PROCESSENTRY32W* lppe);
    BOOL Process32NextW(HANDLE hSnapshot, PROCESSENTRY32W* lppe);
    HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
    BOOL CloseHandle(HANDLE hObject);
    BOOL QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, DWORD* lpdwSize);
    BOOL CreateProcessW(LPCWSTR, LPWSTR, PVOID, PVOID, BOOL, DWORD, PVOID, LPCWSTR, STARTUPINFOW*, PROCESS_INFORMATION*);
    BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);
    DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
    DWORD GetLastError();
    void SetLastError(DWORD dwErrCode);
    BOOL ProcessIdToSessionId(DWORD dwProcessId, DWORD* pSessionId);
    BOOL ReadProcessMemory(HANDLE, const void*, void*, size_t, size_t*);
    BOOL SetPriorityClass(HANDLE hProcess, DWORD dwPriorityClass);
    BOOL CreateProcessAsUserW(HANDLE, LPCWSTR, LPWSTR, PVOID, PVOID, BOOL, DWORD, PVOID, LPCWSTR, STARTUPINFOW*, PROCESS_INFORMATION*);
    BOOL DuplicateTokenEx(HANDLE, DWORD, PVOID, SECURITY_IMPERSONATION_LEVEL, TOKEN_TYPE, HANDLE*);
    int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, const char* lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
    int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, BOOL* lpUsedDefaultChar);
    DWORD GetCurrentProcessId();
    ULONGLONG GetTickCount64(void);
    DWORD FormatMessageW(DWORD dwFlags, const void* lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, void* Arguments);
    HANDLE LocalFree(HANDLE hMem);
    void Sleep(DWORD dwMilliseconds);

    DWORD GetModuleFileNameExW(HANDLE hProcess, HANDLE hModule, LPWSTR lpFilename, DWORD nSize);
    BOOL GetProcessMemoryInfo(HANDLE Process, PVOID ppsmemCounters, DWORD cb);
    long __stdcall NtQueryInformationProcess(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, ULONG*);
    DWORD WTSGetActiveConsoleSessionId(void);
    BOOL WTSQueryUserToken(ULONG SessionId, HANDLE *phToken);
    BOOL CreateEnvironmentBlock(PVOID *lpEnvironment, HANDLE hToken, BOOL bInherit);
    BOOL DestroyEnvironmentBlock(PVOID lpEnvironment);
]]

local C = ffi.C
local INVALID_HANDLE_VALUE = ffi.cast("HANDLE", -1)
local kernel32 = ffi.load("kernel32")
local psapi = ffi.load("psapi")
local ntdll = ffi.load("ntdll")
local wtsapi32 = ffi.load("wtsapi32")
local userenv = ffi.load("userenv")
local advapi32 = ffi.load("advapi32")

local _M = {
    _VERSION = "3.1.0",
    _OS_SUPPORT = true,
}

_M.constants = {
    SW_HIDE             = 0,
    SW_SHOWNORMAL       = 1,
    SW_SHOWMINIMIZED    = 2,
    SW_SHOWMAXIMIZED    = 3,
    SW_SHOW             = 5,
    PROCESS_ALL_ACCESS              = 0x1F0FFF,
    PROCESS_TERMINATE               = 0x0001,
    PROCESS_QUERY_INFORMATION       = 0x0400,
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000,
    PROCESS_VM_READ                 = 0x0010,
    PROCESS_SET_INFORMATION         = 0x0200,
    SYNCHRONIZE                     = 0x00100000,
    PRIORITY_CLASS_IDLE          = 0x00000040,
    PRIORITY_CLASS_BELOW_NORMAL  = 0x00004000,
    PRIORITY_CLASS_NORMAL        = 0x00000020,
    PRIORITY_CLASS_ABOVE_NORMAL  = 0x00008000,
    PRIORITY_CLASS_HIGH          = 0x00000080,
    PRIORITY_CLASS_REALTIME      = 0x00000100,
}

-- All former C-API and helper functions are now module-private 'local' functions.
-- They are the implementation details.
local wstr_to_str, str_to_wstr, formatWinError
local forEachProcess, findProcess, getProcessPath, getProcessCommandLine, terminateProcessTree
local _openProcessByPid, _terminateProcessByPid, _processExists, _findAllProcesses
local _createProcess, _createProcessAsSystem, _getProcessInfo
local _wait, _waitClose, _waitForExit, _setProcessPriority

local PROCESSENTRY32W_T = ffi.typeof("PROCESSENTRY32W")

wstr_to_str = function(wstr_ptr)
    if wstr_ptr == nil or ffi.cast("void*", wstr_ptr) == nil then return "" end
    local required_size = kernel32.WideCharToMultiByte(C.CP_UTF8, 0, wstr_ptr, -1, nil, 0, nil, nil)
    if required_size <= 0 then return "" end
    local buf = ffi.new("char[?]", required_size)
    kernel32.WideCharToMultiByte(C.CP_UTF8, 0, wstr_ptr, -1, buf, required_size, nil, nil)
    return ffi.string(buf, required_size - 1)
end

str_to_wstr = function(str)
    if not str or str == "" then return nil end
    local required_size = kernel32.MultiByteToWideChar(C.CP_UTF8, 0, str, -1, nil, 0)
    if required_size == 0 then return nil end
    local buf = ffi.new("WCHAR[?]", required_size)
    kernel32.MultiByteToWideChar(C.CP_UTF8, 0, str, -1, buf, required_size)
    return buf
end

formatWinError = function(err_code)
    if err_code == 0 then return "" end
    local flags = C.FORMAT_MESSAGE_ALLOCATE_BUFFER + C.FORMAT_MESSAGE_FROM_SYSTEM + C.FORMAT_MESSAGE_IGNORE_INSERTS
    local buf_ptr = ffi.new("LPWSTR[1]")
    local len = kernel32.FormatMessageW(flags, nil, err_code, 0, ffi.cast("LPWSTR", buf_ptr), 0, nil)
    if len > 0 then
        local msg_handle = buf_ptr[0]
        local managed_handle = ffi.gc(msg_handle, kernel32.LocalFree)
        return wstr_to_str(managed_handle):gsub("[\r\n]+$", "")
    end
    return "Unknown error code " .. tostring(err_code)
end

forEachProcess = function(callback)
    local h_snap = kernel32.CreateToolhelp32Snapshot(C.TH32CS_SNAPPROCESS, 0)
    if h_snap == INVALID_HANDLE_VALUE then
        return false, kernel32.GetLastError()
    end
    local snap_handle = ffi.gc(h_snap, kernel32.CloseHandle)
    local pe = PROCESSENTRY32W_T()
    pe.dwSize = ffi.sizeof(pe)
    
    if kernel32.Process32FirstW(snap_handle, pe) ~= 0 then
        repeat
            if callback(pe) then break end
        until kernel32.Process32NextW(snap_handle, pe) == 0
    end
    return true
end

findProcess = function(name_or_pid)
    if not name_or_pid then return 0 end
    local target_pid_num = tonumber(name_or_pid)
    local is_pid_search = (target_pid_num ~= nil)
    local target_name_lower = is_pid_search and "" or name_or_pid:lower()
    local found_pid = 0
    forEachProcess(function(pe)
        if is_pid_search then
            if pe.th32ProcessID == target_pid_num then
                found_pid = pe.th32ProcessID
                return true
            end
        else
            if wstr_to_str(pe.szExeFile):lower() == target_name_lower then
                found_pid = pe.th32ProcessID
                return true
            end
        end
    end)
    return found_pid
end

getProcessPath = function(pid, buffer, buffer_size, process_handle)
    local h_proc = process_handle
    local handle_to_close
    if not h_proc then
        h_proc = kernel32.OpenProcess(C.PROCESS_QUERY_LIMITED_INFORMATION, false, pid)
        if h_proc == nil or h_proc == INVALID_HANDLE_VALUE then return 0 end
        handle_to_close = ffi.gc(h_proc, kernel32.CloseHandle)
    end
    local path_length_ptr = ffi.new("DWORD[1]", buffer_size)
    if kernel32.QueryFullProcessImageNameW(h_proc, 0, buffer, path_length_ptr) ~= 0 then
        return path_length_ptr[0]
    end
    local path_length = psapi.GetModuleFileNameExW(h_proc, nil, buffer, buffer_size)
    return path_length > 0 and path_length or 0
end

getProcessCommandLine = function(pid, buffer, buffer_size)
    buffer[0] = 0
    local h_proc = kernel32.OpenProcess(C.PROCESS_QUERY_INFORMATION + C.PROCESS_VM_READ, false, pid)
    if not h_proc or h_proc == INVALID_HANDLE_VALUE then return false end
    local proc_handle = ffi.gc(h_proc, kernel32.CloseHandle)
    local pbi = ffi.new("PROCESS_BASIC_INFORMATION")
    if ntdll.NtQueryInformationProcess(proc_handle, C.ProcessBasicInformation, pbi, ffi.sizeof(pbi), nil) ~= 0 then return false end
    if pbi.PebBaseAddress == nil then return false end
    local peb = ffi.new("PEB")
    if kernel32.ReadProcessMemory(proc_handle, pbi.PebBaseAddress, peb, ffi.sizeof(peb), nil) == 0 then return false end
    if peb.ProcessParameters == nil then return false end
    local params = ffi.new("RTL_USER_PROCESS_PARAMETERS")
    if kernel32.ReadProcessMemory(proc_handle, peb.ProcessParameters, params, ffi.sizeof(params), nil) == 0 then return false end
    if params.CommandLine.Length > 0 then
        local bytes_to_read = params.CommandLine.Length > (buffer_size - 1) * 2 and (buffer_size - 1) * 2 or params.CommandLine.Length
        if kernel32.ReadProcessMemory(proc_handle, params.CommandLine.Buffer, buffer, bytes_to_read, nil) ~= 0 then
            buffer[bytes_to_read / 2] = 0
            return true
        end
    end
    return false
end

_terminateProcessByPid = function(pid, exit_code)
    if pid == 0 then kernel32.SetLastError(C.ERROR_INVALID_PARAMETER); return false end
    local h_proc = kernel32.OpenProcess(C.PROCESS_TERMINATE, false, pid)
    if not h_proc or h_proc == INVALID_HANDLE_VALUE then return false end
    local proc_handle = ffi.gc(h_proc, kernel32.CloseHandle)
    return kernel32.TerminateProcess(proc_handle, exit_code or 0) ~= 0
end

terminateProcessTree = function(pid)
    forEachProcess(function(pe)
        if pe.th32ParentProcessID == pid then
            terminateProcessTree(pe.th32ProcessID)
        end
    end)
    _terminateProcessByPid(pid, 1)
end

_openProcessByPid = function(pid, desired_access)
    if pid == 0 then kernel32.SetLastError(C.ERROR_INVALID_PARAMETER); return nil end
    local h_proc = kernel32.OpenProcess(desired_access, false, pid)
    return (h_proc ~= nil and h_proc ~= INVALID_HANDLE_VALUE) and h_proc or nil
end

_processExists = function(process_name_or_pid)
    if not process_name_or_pid or process_name_or_pid == "" then kernel32.SetLastError(C.ERROR_INVALID_PARAMETER); return 0 end
    return findProcess(process_name_or_pid)
end

_findAllProcesses = function(process_name, out_pids, pids_array_size)
    if not process_name or process_name == "" or (not out_pids and pids_array_size > 0) then kernel32.SetLastError(C.ERROR_INVALID_PARAMETER); return -1 end
    local found_count, stored_count = 0, 0
    local target_name_lower = process_name:lower()
    forEachProcess(function(pe)
        if wstr_to_str(pe.szExeFile):lower() == target_name_lower then
            if out_pids and stored_count < pids_array_size then
                out_pids[stored_count] = pe.th32ProcessID
                stored_count = stored_count + 1
            end
            found_count = found_count + 1
        end
    end)
    return out_pids and stored_count or found_count
end

_createProcess = function(command, working_dir, show_mode, desktop_name)
    local result = ffi.new("ProcUtils_ProcessResult")
    if not command or command == "" then result.last_error_code = C.ERROR_INVALID_PARAMETER; return result end
    local si = ffi.new("STARTUPINFOW"); si.cb = ffi.sizeof(si)
    si.dwFlags, si.wShowWindow = C.STARTF_USESHOWWINDOW, show_mode or _M.constants.SW_SHOW
    if desktop_name and desktop_name ~= "" then si.lpDesktop = str_to_wstr(desktop_name) end
    local pi = ffi.new("PROCESS_INFORMATION")
    local cmd_buffer_w = str_to_wstr(command)
    local wd_wstr = str_to_wstr(working_dir)
    if kernel32.CreateProcessW(nil, cmd_buffer_w, nil, nil, false, 0, nil, wd_wstr, si, pi) == 0 then 
        result.last_error_code = kernel32.GetLastError()
        return result 
    end
    kernel32.CloseHandle(pi.hThread)
    result.pid, result.process_handle, result.last_error_code = pi.dwProcessId, pi.hProcess, 0
    return result
end

_createProcessAsSystem = function(command, working_dir, show_mode)
    local result = ffi.new("ProcUtils_ProcessResult")
    local last_error
    repeat
        if not command or command == "" then last_error = C.ERROR_INVALID_PARAMETER; break end
        local session_id = kernel32.WTSGetActiveConsoleSessionId()
        if session_id == 0xFFFFFFFF then last_error = kernel32.GetLastError(); if last_error == 0 then last_error = 1008 end; break end
        local user_token_ptr = ffi.new("HANDLE[1]")
        if wtsapi32.WTSQueryUserToken(session_id, user_token_ptr) == 0 then last_error = kernel32.GetLastError(); if last_error == 0 then last_error = 1008 end; break end
        local user_token = ffi.gc(user_token_ptr[0], kernel32.CloseHandle)
        local primary_token_ptr = ffi.new("HANDLE[1]")
        if advapi32.DuplicateTokenEx(user_token, C.MAXIMUM_ALLOWED, nil, C.SecurityIdentification, C.TokenPrimary, primary_token_ptr) == 0 then last_error = kernel32.GetLastError(); if last_error == 0 then last_error = 5 end; break end
        local primary_token = ffi.gc(primary_token_ptr[0], kernel32.CloseHandle)
        local env_block_ptr = ffi.new("PVOID[1]")
        if userenv.CreateEnvironmentBlock(env_block_ptr, primary_token, false) == 0 then last_error = kernel32.GetLastError(); if last_error == 0 then last_error = 1157 end; break end
        local env_block = ffi.gc(env_block_ptr[0], userenv.DestroyEnvironmentBlock)
        local si = ffi.new("STARTUPINFOW"); si.cb = ffi.sizeof(si)
        si.dwFlags, si.wShowWindow = C.STARTF_USESHOWWINDOW, show_mode or _M.constants.SW_SHOW
        si.lpDesktop = str_to_wstr("winsta0\\default")
        local pi = ffi.new("PROCESS_INFORMATION")
        local cmd_buffer_w = str_to_wstr(command)
        local wd_wstr = str_to_wstr(working_dir)
        if kernel32.CreateProcessAsUserW(primary_token, nil, cmd_buffer_w, nil, nil, false, C.CREATE_UNICODE_ENVIRONMENT, env_block, wd_wstr, si, pi) == 0 then last_error = kernel32.GetLastError(); if last_error == 0 then last_error = 1157 end; break end
        kernel32.CloseHandle(pi.hThread)
        result.pid, result.process_handle, last_error = pi.dwProcessId, pi.hProcess, 0
    until true
    result.last_error_code = last_error
    if last_error and last_error ~= 0 then kernel32.SetLastError(last_error) end
    return result
end

_getProcessInfo = function(pid, out_info)
    if pid == 0 or out_info == nil then kernel32.SetLastError(C.ERROR_INVALID_PARAMETER); return false end
    ffi.fill(out_info, ffi.sizeof(out_info))
    local h_proc = kernel32.OpenProcess(C.PROCESS_QUERY_INFORMATION + C.PROCESS_VM_READ, false, pid)
    if not h_proc or h_proc == INVALID_HANDLE_VALUE then return false end
    local proc_handle = ffi.gc(h_proc, kernel32.CloseHandle)
    local found = false
    forEachProcess(function(pe) if pe.th32ProcessID == pid then out_info.parent_pid, out_info.thread_count, found = pe.th32ParentProcessID, pe.cntThreads, true; return true end end)
    if not found then return false end
    local session_id_ptr = ffi.new("DWORD[1]")
    if kernel32.ProcessIdToSessionId(pid, session_id_ptr) ~= 0 then out_info.session_id = session_id_ptr[0] else out_info.session_id = -1 end
    local pmc = ffi.new("PROCESS_MEMORY_COUNTERS_EX"); pmc.cb = ffi.sizeof(pmc)
    if psapi.GetProcessMemoryInfo(proc_handle, pmc, ffi.sizeof(pmc)) ~= 0 then out_info.memory_usage_bytes = pmc.WorkingSetSize end
    getProcessPath(pid, out_info.exe_path, 260, proc_handle)
    getProcessCommandLine(pid, out_info.command_line, 2048)
    out_info.pid = pid
    return true
end

_waitForExit = function(process_handle, timeout_ms)
    if process_handle == nil or process_handle == INVALID_HANDLE_VALUE then kernel32.SetLastError(C.ERROR_INVALID_PARAMETER); return false end
    local res = kernel32.WaitForSingleObject(process_handle, timeout_ms < 0 and C.INFINITE or timeout_ms)
    if res == C.WAIT_TIMEOUT then kernel32.SetLastError(C.WAIT_TIMEOUT) end
    return res == C.WAIT_OBJECT_0
end

_setProcessPriority = function(pid, priority)
    local pmap = {L=C.IDLE_PRIORITY_CLASS,B=C.BELOW_NORMAL_PRIORITY_CLASS,N=C.NORMAL_PRIORITY_CLASS,A=C.ABOVE_NORMAL_PRIORITY_CLASS,H=C.HIGH_PRIORITY_CLASS,R=C.REALTIME_PRIORITY_CLASS}
    local pclass = pmap[priority and priority:upper() or ""]
    if not pclass then
        kernel32.SetLastError(C.ERROR_INVALID_PARAMETER)
        return false
    end
    if not pid or pid == 0 then
        kernel32.SetLastError(C.ERROR_NOT_FOUND)
        return false
    end
    local h_proc = kernel32.OpenProcess(C.PROCESS_SET_INFORMATION, false, pid)
    if not h_proc or h_proc == INVALID_HANDLE_VALUE then
        return false
    end
    local proc_handle = ffi.gc(h_proc, kernel32.CloseHandle)
    return kernel32.SetPriorityClass(proc_handle, pclass) ~= 0
end

_wait = function(process_name, timeout_ms)
    if not process_name or process_name == "" then
        kernel32.SetLastError(C.ERROR_INVALID_PARAMETER)
        return 0
    end
    local start = ffi.C.GetTickCount64()
    timeout_ms = timeout_ms or -1
    while true do
        local pid = findProcess(process_name)
        if pid > 0 then return pid end
        if timeout_ms >= 0 and (ffi.C.GetTickCount64() - start) >= timeout_ms then
            kernel32.SetLastError(C.WAIT_TIMEOUT)
            return 0
        end
        ffi.C.Sleep(100)
    end
end

-------------------------------------------------------------------------------
-- High-Level OOP Wrapper (The Only Public API)
-------------------------------------------------------------------------------

local Process = {}
Process.__index = Process

local function new_process(pid, handle)
    local self = {
        pid = pid,
        _handle = handle,
    }
    return setmetatable(self, Process)
end

function Process:__gc()
    if self._handle and self._handle ~= INVALID_HANDLE_VALUE then
        kernel32.CloseHandle(self._handle)
        self._handle = nil
    end
end

function _M.exec(command, working_dir, show_mode, desktop_name)
    local result = _createProcess(command, working_dir, show_mode, desktop_name)
    if result.pid > 0 then
        return new_process(result.pid, result.process_handle)
    end
    local err_code = result.last_error_code
    return nil, err_code, formatWinError(err_code)
end

function _M.exec_as_system(command, working_dir, show_mode)
    local result = _createProcessAsSystem(command, working_dir, show_mode)
    if result.pid > 0 then
        return new_process(result.pid, result.process_handle)
    end
    local err_code = result.last_error_code
    return nil, err_code, formatWinError(err_code)
end

function _M.open_by_pid(pid, access)
    access = access or _M.constants.PROCESS_ALL_ACCESS
    local handle = _openProcessByPid(pid, access)
    if handle then
        return new_process(pid, handle)
    end
    local err_code = kernel32.GetLastError()
    return nil, err_code, formatWinError(err_code)
end

function _M.open_by_name(name, access)
    access = access or _M.constants.PROCESS_ALL_ACCESS
    local pid = _processExists(name)
    if pid > 0 then
        return _M.open_by_pid(pid, access)
    end
    local err_code = kernel32.GetLastError()
    if err_code == 0 then err_code = C.ERROR_NOT_FOUND end
    return nil, err_code, formatWinError(err_code)
end

function _M.current()
    local pid = ffi.C.GetCurrentProcessId()
    return _M.open_by_pid(pid)
end

function Process:is_valid()
    return self._handle and self._handle ~= INVALID_HANDLE_VALUE
end

function Process:handle()
    return self._handle
end

function Process:terminate(exit_code)
    if not _terminateProcessByPid(self.pid, exit_code or 0) then
        local err_code = kernel32.GetLastError()
        return nil, err_code, formatWinError(err_code)
    end
    return true
end

function Process:terminate_tree()
    terminateProcessTree(self.pid)
    return true
end

function Process:wait_for_exit(timeout_ms)
    return _waitForExit(self._handle, timeout_ms or -1)
end

function Process:get_info()
    local info_cdata = ffi.new("ProcUtils_ProcessInfo")
    if _getProcessInfo(self.pid, info_cdata) then
        return {
            pid = info_cdata.pid,
            parent_pid = info_cdata.parent_pid,
            session_id = info_cdata.session_id,
            exe_path = wstr_to_str(info_cdata.exe_path),
            command_line = wstr_to_str(info_cdata.command_line),
            memory_usage_bytes = tonumber(info_cdata.memory_usage_bytes),
            thread_count = info_cdata.thread_count,
        }
    end
    local err_code = kernel32.GetLastError()
    return nil, err_code, formatWinError(err_code)
end

function Process:get_path()
    local buffer = ffi.new("WCHAR[260]")
    if getProcessPath(self.pid, buffer, 260, self._handle) > 0 then
        return wstr_to_str(buffer)
    end
    local err_code = kernel32.GetLastError()
    return nil, err_code, formatWinError(err_code)
end

function Process:get_command_line()
    local buffer = ffi.new("WCHAR[2048]")
    if getProcessCommandLine(self.pid, buffer, 2048) then
        return wstr_to_str(buffer)
    end
    local err_code = kernel32.GetLastError()
    return nil, err_code, formatWinError(err_code)
end

function Process:set_priority(priority)
    if not _setProcessPriority(self.pid, priority) then
        local err_code = kernel32.GetLastError()
        return nil, err_code, formatWinError(err_code)
    end
    return true
end

function _M.find_all(name)
    local count = _findAllProcesses(name, nil, 0)
    if count < 0 then 
        local err_code = kernel32.GetLastError()
        return nil, err_code, formatWinError(err_code)
    end
    if count == 0 then return {} end
    local buffer = ffi.new("DWORD[?]", count)
    local stored = _findAllProcesses(name, buffer, count)
    local pids = {}
    for i = 0, stored - 1 do
        pids[i + 1] = buffer[i]
    end
    return pids
end

function _M.exists(name_or_pid)
    return _processExists(name_or_pid)
end

function _M.terminate_by_pid(pid, exit_code)
    return _terminateProcessByPid(pid, exit_code or 0)
end

function _M.wait(process_name, timeout_ms)
    local pid = _wait(process_name, timeout_ms)
    if pid > 0 then
        return pid
    end
    local err_code = kernel32.GetLastError()
    if err_code == 0 then err_code = C.WAIT_TIMEOUT end -- Ensure error code on timeout
    return nil, err_code, formatWinError(err_code)
end

function _M.wait_close(process_name_or_pid, timeout_ms)
    if not process_name_or_pid or process_name_or_pid == "" then return false end
    local pid = findProcess(process_name_or_pid); if pid == 0 then return true end
    local h_proc = _openProcessByPid(pid, _M.constants.SYNCHRONIZE)
    if not h_proc then return false end
    local proc_handle = ffi.gc(h_proc, kernel32.CloseHandle)
    return _waitForExit(proc_handle, timeout_ms or -1)
end

return _M