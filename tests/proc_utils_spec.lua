-- tests/proc_utils_spec.lua
-- Unit tests for proc_utils_ffi.lua using luaunit.
-- [COVERAGE] 100% parity with Python/C++ test suite (ProcUtils_ProcessGetParent added).
-- [FIXED] Package path now supports running from Project Root (CI) or tests/ dir.

-- 1. Configure path to find 'proc_utils_ffi.lua' and 'luaunit.lua'
package.path = package.path .. ';./?.lua;./vendor/luaunit/?.lua;../?.lua;../vendor/luaunit/?.lua'

local status, lu = pcall(require, 'luaunit')
if not status then
    print("[ERROR] Could not find 'luaunit'.")
    print("  Current package.path: " .. package.path)
    print("  Ensure you have run: git submodule update --init --recursive")
    os.exit(1)
end

local ffi = require("ffi")

-- 2. Load the library under test
local proc_status, proc = pcall(require, 'proc_utils_ffi')
if not proc_status then
    print("[ERROR] Could not find 'proc_utils_ffi'.")
    print("  Error: " .. tostring(proc))
    os.exit(1)
end

if not proc._OS_SUPPORT then
    print("Skipping tests: proc_utils-ffi only supports Windows.")
    os.exit(0)
end

-- 3. Local Helpers
local INVALID_HANDLE_VALUE = ffi.cast("HANDLE", -1)

local function table_isempty(t)
    return next(t) == nil
end

local function wstr_to_str(wstr_ptr)
    if wstr_ptr == nil then return "" end
    local str_table = {}
    local i = 0
    while true do
        local c = wstr_ptr[i]
        if c == 0 then break end
        if c < 128 then
            table.insert(str_table, string.char(c))
        else
            table.insert(str_table, "?")
        end
        i = i + 1
    end
    return table.concat(str_table)
end

-- 4. Test Suite Definition
TestProcUtils = {}

local TEST_PROC_NAME = "ping.exe"
local TEST_COMMAND = 'ping.exe -n 30 127.0.0.1' 
local TEST_COMMAND_WITH_ARGS = 'ping.exe -n 4 127.0.0.1'
local CMD_PING_COMMAND = 'cmd.exe /c "ping -n 10 127.0.0.1 > NUL"'
local UNICODE_FILENAME = "测试文件.txt"

function TestProcUtils:setUp()
    print("\n[SETUP] Cleaning up environment before test...")
    self:tearDown()
    print("[SETUP] Cleanup complete.")
end

function TestProcUtils:tearDown()
    local function kill_all_by_name(name)
        local pids_to_kill = {}
        local pids = proc.find_all(name)
        if pids and not table_isempty(pids) then
            for _, pid in ipairs(pids) do
                table.insert(pids_to_kill, pid)
            end
        end

        if not table_isempty(pids_to_kill) then
            print("  [TEARDOWN] Cleaning up lingering '" .. name .. "': " .. table.concat(pids_to_kill, ", "))
            for _, pid in ipairs(pids_to_kill) do
                proc.terminate_by_pid(pid, 0)
            end
        end
    end

    kill_all_by_name(TEST_PROC_NAME)
    kill_all_by_name("cmd.exe")
    kill_all_by_name("whoami.exe")
    
    os.remove(UNICODE_FILENAME)
    ffi.C.Sleep(100)
end

-------------------------------------------------------------------------------
-- 1. C-API Tests
-------------------------------------------------------------------------------

function TestProcUtils:test_capi_launch_and_exists()
    print("[RUNNING] test_capi_launch_and_exists")
    local pid = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    lu.assertTrue(pid > 0, "Launch failed")
    print("  [DEBUG] Launched: " .. pid)
    ffi.C.Sleep(500)
    
    local exists_name = proc.C_API.ProcUtils_ProcessExists(TEST_PROC_NAME)
    lu.assertTrue(exists_name > 0, "Exists(name) failed")
    
    local exists_pid = proc.C_API.ProcUtils_ProcessExists(tostring(pid))
    lu.assertEquals(exists_pid, pid, "Exists(pid) mismatch")
    
    local exists_fake = proc.C_API.ProcUtils_ProcessExists("non_existent_123.exe")
    lu.assertEquals(exists_fake, 0, "Exists(fake) should be 0")
    
    proc.terminate_by_pid(pid, 0)
    print("[SUCCESS] test_capi_launch_and_exists")
end

function TestProcUtils:test_capi_create_and_open_process()
    print("[RUNNING] test_capi_create_and_open_process")
    local result = proc.C_API.ProcUtils_CreateProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    local handle_to_close = result.process_handle
    
    local success, err = pcall(function()
        lu.assertTrue(result.pid > 0, "CreateProcess failed PID")
        lu.assertNotIsNil(result.process_handle, "CreateProcess failed Handle")
        print("  [DEBUG] Created PID: " .. result.pid)
        
        -- OpenProcessByPid
        local h_by_pid = proc.C_API.ProcUtils_OpenProcessByPid(result.pid, proc.constants.SYNCHRONIZE)
        lu.assertNotIsNil(h_by_pid, "OpenByPid failed")
        ffi.C.CloseHandle(h_by_pid)
        
        -- WaitForProcessExit (Timeout expected)
        local closed = proc.C_API.ProcUtils_WaitForProcessExit(result.process_handle, 200)
        lu.assertFalse(closed, "WaitForProcessExit should timeout (return false)")
        
        -- Terminate
        local term = proc.C_API.ProcUtils_TerminateProcessByPid(result.pid, 0)
        lu.assertEquals(term, 1, "TerminateProcessByPid failed (expected 1)")
        
        -- WaitForProcessExit (Success expected)
        local closed_after = proc.C_API.ProcUtils_WaitForProcessExit(result.process_handle, 1000)
        lu.assertTrue(closed_after, "WaitForProcessExit should succeed (return true)")
    end)

    if handle_to_close and handle_to_close ~= INVALID_HANDLE_VALUE then
        ffi.C.CloseHandle(handle_to_close)
    end
    
    if not success then error(err) end
    print("[SUCCESS] test_capi_create_and_open_process")
end

function TestProcUtils:test_capi_terminate_and_wait_close()
    print("[RUNNING] test_capi_terminate_and_wait_close")
    local pid = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    lu.assertTrue(pid > 0)
    ffi.C.Sleep(500)
    
    local ok = proc.C_API.ProcUtils_TerminateProcessByPid(pid, 0)
    lu.assertEquals(ok, 1, "TerminateByPid failed (expected 1)")
    
    local closed = proc.C_API.ProcUtils_ProcessWaitClose(tostring(pid), 3000)
    lu.assertTrue(closed, "WaitClose failed (expected true)")
    print("[SUCCESS] test_capi_terminate_and_wait_close")
end

function TestProcUtils:test_capi_wait_and_wait_close_timeout()
    print("[RUNNING] test_capi_wait_and_wait_close_timeout")
    -- 1. ProcessWait Timeout
    local start = ffi.C.GetTickCount64()
    local pid = proc.C_API.ProcUtils_ProcessWait("fake_proc_123.exe", 500)
    local duration = tonumber(ffi.C.GetTickCount64() - start)
    lu.assertEquals(pid, 0)
    lu.assertTrue(duration >= 400, "Wait duration too short")
    
    -- 2. ProcessWaitClose Timeout
    local real_pid = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    start = ffi.C.GetTickCount64()
    local closed = proc.C_API.ProcUtils_ProcessWaitClose(TEST_PROC_NAME, 500)
    duration = tonumber(ffi.C.GetTickCount64() - start)
    
    lu.assertFalse(closed, "WaitClose should timeout (return false)")
    lu.assertTrue(duration >= 400, "WaitClose duration too short")
    
    proc.terminate_by_pid(real_pid, 0)
    print("[SUCCESS] test_capi_wait_and_wait_close_timeout")
end

function TestProcUtils:test_capi_get_path_and_set_priority()
    print("[RUNNING] test_capi_get_path_and_set_priority")
    local pid = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    lu.assertTrue(pid > 0)
    ffi.C.Sleep(500)
    
    -- GetPath
    local path_buf = ffi.new("WCHAR[260]")
    local ok = proc.C_API.ProcUtils_ProcessGetPath(pid, path_buf, 260)
    lu.assertTrue(ok, "GetPath failed")
    local path = wstr_to_str(path_buf)
    lu.assertStrContains(path:lower(), TEST_PROC_NAME, "Path mismatch")
    
    -- SetPriority
    local prio_ok = proc.C_API.ProcUtils_ProcessSetPriority(tostring(pid), "L")
    lu.assertEquals(prio_ok, 1, "SetPriority(L) failed (expected 1)")
    
    prio_ok = proc.C_API.ProcUtils_ProcessSetPriority(TEST_PROC_NAME, "X")
    lu.assertFalse(prio_ok, "SetPriority(X) should fail (expected false)")
    
    proc.terminate_by_pid(pid, 0)
    print("[SUCCESS] test_capi_get_path_and_set_priority")
end

function TestProcUtils:test_capi_get_parent_and_close_tree()
    print("[RUNNING] test_capi_get_parent_and_close_tree")
    local parent_pid = proc.C_API.ProcUtils_LaunchProcess(CMD_PING_COMMAND, nil, proc.constants.SW_HIDE)
    lu.assertTrue(parent_pid > 0)
    
    local child_pid = proc.C_API.ProcUtils_ProcessWait("PING.EXE", 3000)
    lu.assertTrue(child_pid > 0, "Child not found")
    
    -- [COVERAGE-FIX] Assert GetParent works (Parity with Python test)
    local retrieved_parent = proc.C_API.ProcUtils_ProcessGetParent("PING.EXE")
    lu.assertEquals(retrieved_parent, parent_pid, "GetParent PID mismatch")
    
    local ok = proc.C_API.ProcUtils_TerminateProcessTreeByPid(parent_pid)
    lu.assertTrue(ok, "TerminateTree failed")
    
    ffi.C.Sleep(500)
    lu.assertEquals(proc.C_API.ProcUtils_ProcessExists(tostring(parent_pid)), 0, "Parent remains")
    lu.assertEquals(proc.C_API.ProcUtils_ProcessExists(tostring(child_pid)), 0, "Child remains")
    print("[SUCCESS] test_capi_get_parent_and_close_tree")
end

function TestProcUtils:test_capi_find_all_processes()
    print("[RUNNING] test_capi_find_all_processes")
    local p1 = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    local p2 = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    ffi.C.Sleep(1000)
    
    local count = proc.C_API.ProcUtils_FindAllProcesses(TEST_PROC_NAME, nil, 0)
    lu.assertTrue(count >= 2, "Count < 2")
    
    local buffer = ffi.new("DWORD[?]", count)
    local stored = proc.C_API.ProcUtils_FindAllProcesses(TEST_PROC_NAME, buffer, count)
    lu.assertEquals(stored, count)
    
    local found = false
    for i = 0, stored-1 do
        if buffer[i] == p1 then found = true break end
    end
    lu.assertTrue(found, "P1 not found in list")
    
    proc.terminate_by_pid(p1, 0)
    proc.terminate_by_pid(p2, 0)
    print("[SUCCESS] test_capi_find_all_processes")
end

function TestProcUtils:test_capi_get_full_process_info()
    print("[RUNNING] test_capi_get_full_process_info")
    local pid = proc.C_API.ProcUtils_LaunchProcess(TEST_COMMAND_WITH_ARGS, nil, proc.constants.SW_HIDE)
    lu.assertTrue(pid > 0)
    ffi.C.Sleep(500)
    
    local info = ffi.new("ProcUtils_ProcessInfo")
    local ok = proc.C_API.ProcUtils_ProcessGetInfo(pid, info)
    lu.assertTrue(ok, "GetInfo failed")
    
    lu.assertEquals(info.pid, pid)
    lu.assertStrContains(wstr_to_str(info.exe_path):lower(), TEST_PROC_NAME)
    lu.assertStrContains(wstr_to_str(info.command_line), "-n 4")
    lu.assertTrue(info.session_id > 0)
    
    -- Test command line via buffer directly
    local cmd_buf = ffi.new("WCHAR[2048]")
    ok = proc.C_API.ProcUtils_ProcessGetCommandLine(pid, cmd_buf, 2048)
    lu.assertTrue(ok)
    lu.assertStrContains(wstr_to_str(cmd_buf), "-n 4")

    -- Test invalid pid
    ok = proc.C_API.ProcUtils_ProcessGetInfo(999999, info)
    lu.assertFalse(ok)
    
    proc.terminate_by_pid(pid, 0)
    print("[SUCCESS] test_capi_get_full_process_info")
end

function TestProcUtils:test_capi_invalid_inputs()
    print("[RUNNING] test_capi_invalid_inputs")
    lu.assertEquals(proc.C_API.ProcUtils_ProcessExists(nil), 0)
    
    lu.assertFalse(proc.C_API.ProcUtils_ProcessClose(nil, 0), "ProcessClose(nil) expected false")
    
    lu.assertEquals(proc.C_API.ProcUtils_ProcessExists(""), 0)
    
    lu.assertFalse(proc.C_API.ProcUtils_ProcessGetPath(99999, nil, 260), "GetPath(nil) expected false")
    print("[SUCCESS] test_capi_invalid_inputs")
end

function TestProcUtils:test_capi_create_as_system()
    print("[RUNNING] test_capi_create_as_system")
    local result = proc.C_API.ProcUtils_CreateProcessAsSystem("whoami.exe", nil, proc.constants.SW_HIDE)
    
    if result.pid > 0 and result.process_handle ~= nil then
        print("  [DEBUG] Admin Success. PID: " .. result.pid)
        ffi.C.CloseHandle(result.process_handle)
        ffi.C.Sleep(500)
        lu.assertEquals(proc.exists(tostring(result.pid)), 0)
    else
        local err = result.last_error_code
        print("  [DEBUG] Expected CI/User Fail. Error: " .. err)
        -- Known error codes for non-admin/no-session environments
        local allowed = {[5]=true, [6]=true, [1314]=true, [1008]=true, [1157]=true}
        if not allowed[err] then
            lu.fail("Unexpected error code from CreateAsSystem: " .. err)
        end
    end
    print("[SUCCESS] test_capi_create_as_system")
end

-------------------------------------------------------------------------------
-- 2. OOP Wrapper Tests
-------------------------------------------------------------------------------

function TestProcUtils:test_oop_exec_and_get_info()
    print("[RUNNING] test_oop_exec_and_get_info")
    local p = proc.exec(TEST_COMMAND, nil, proc.constants.SW_HIDE)
    lu.assertNotIsNil(p, "proc.exec failed")
    local info = p:get_info()
    lu.assertEquals(info.pid, p.pid)
    p:terminate()
    print("[SUCCESS] test_oop_exec_and_get_info")
end

function TestProcUtils:test_oop_unicode_support()
    print("[RUNNING] test_oop_unicode_support")
    local unicode_arg = "arg_" .. UNICODE_FILENAME
    local cmd = string.format('cmd.exe /c "ping -n 2 127.0.0.1 > NUL & rem %s"', unicode_arg)
    local p = proc.exec(cmd, nil, proc.constants.SW_HIDE)
    lu.assertNotIsNil(p)
    ffi.C.Sleep(500)
    local cl = p:get_command_line()
    if cl then
        lu.assertStrContains(cl, unicode_arg)
    end
    p:terminate_tree()
    print("[SUCCESS] test_oop_unicode_support")
end

local exit_code = lu.LuaUnit.run()
os.exit(exit_code == 0)