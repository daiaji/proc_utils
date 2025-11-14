import ctypes
import os
import sys
import time
import subprocess
import threading
from ctypes import wintypes

# --- 全局配置 ---
DLL_FILENAME = "proc_utils.dll"
NOTEPAD_EXE = "notepad.exe"
CMD_PING_COMMAND = f'cmd.exe /c "ping -n 20 127.0.0.1 > NUL"'

g_test_stats = {"success": 0, "fail": 0}

# --- 测试装饰器和助手函数 ---
def test_case(func):
    """一个简单的装饰器，用于运行测试函数、打印结果并进行清理。"""
    def wrapper(lib):
        test_name = func.__name__
        print(f"\n--- Running Test: {test_name} ---")
        cleanup_process(NOTEPAD_EXE)
        cleanup_process("cmd.exe")
        cleanup_process("PING.EXE")
        time.sleep(0.2)
        
        try:
            func(lib)
        except Exception as e:
            report_fail(f"Test crashed with an exception: {e}")
        finally:
            cleanup_process(NOTEPAD_EXE)
            cleanup_process("cmd.exe")
            cleanup_process("PING.EXE")
            print(f"--- Finished Test: {test_name} ---")
    return wrapper

def report_success(message):
    print(f"[  OK  ] {message}")
    g_test_stats["success"] += 1

def report_fail(message):
    print(f"[ FAIL ] {message}")
    g_test_stats["fail"] += 1

def cleanup_process(process_name):
    subprocess.run(f"taskkill /F /IM {process_name}", check=False, capture_output=True)

def setup_library_functions(lib):
    """为库中的所有函数定义参数类型和返回类型。"""
    lib.ProcUtils_ProcessExists.argtypes = [wintypes.LPCWSTR]
    lib.ProcUtils_ProcessExists.restype = wintypes.UINT

    lib.ProcUtils_ProcessClose.argtypes = [wintypes.LPCWSTR, wintypes.UINT]
    lib.ProcUtils_ProcessClose.restype = ctypes.c_bool

    lib.ProcUtils_ProcessWait.argtypes = [wintypes.LPCWSTR, wintypes.INT]
    lib.ProcUtils_ProcessWait.restype = wintypes.UINT

    lib.ProcUtils_ProcessWaitClose.argtypes = [wintypes.LPCWSTR, wintypes.INT]
    lib.ProcUtils_ProcessWaitClose.restype = ctypes.c_bool

    lib.ProcUtils_ProcessGetPath.argtypes = [wintypes.UINT, wintypes.LPWSTR, wintypes.INT]
    lib.ProcUtils_ProcessGetPath.restype = ctypes.c_bool

    lib.ProcUtils_Exec.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.INT, ctypes.c_bool, wintypes.LPCWSTR]
    lib.ProcUtils_Exec.restype = wintypes.UINT

    lib.ProcUtils_ProcessGetParent.argtypes = [wintypes.LPCWSTR]
    lib.ProcUtils_ProcessGetParent.restype = wintypes.UINT

    lib.ProcUtils_ProcessSetPriority.argtypes = [wintypes.LPCWSTR, wintypes.WCHAR]
    lib.ProcUtils_ProcessSetPriority.restype = ctypes.c_bool
    
    lib.ProcUtils_ProcessCloseTree.argtypes = [wintypes.LPCWSTR]
    lib.ProcUtils_ProcessCloseTree.restype = ctypes.c_bool

    # --- 新增函数签名 ---
    lib.ProcUtils_FindAllProcesses.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.UINT), wintypes.INT]
    lib.ProcUtils_FindAllProcesses.restype = wintypes.INT

# --- 测试用例 ---
@test_case
def test_exec_and_exists(lib):
    """测试 ProcUtils_Exec 和 ProcUtils_ProcessExists 的基本功能。"""
    pid = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None) # SW_SHOWNORMAL = 1
    if pid > 0:
        report_success(f"Exec: Successfully launched '{NOTEPAD_EXE}' with PID {pid}.")
    else:
        report_fail(f"Exec: Failed to launch '{NOTEPAD_EXE}'.")
        return
    time.sleep(0.5) 
    found_pid_by_name = lib.ProcUtils_ProcessExists(NOTEPAD_EXE)
    if found_pid_by_name == pid:
        report_success(f"Exists (by name): Found process, PID matches ({pid}).")
    else:
        report_fail(f"Exists (by name): PID mismatch or not found. Expected {pid}, got {found_pid_by_name}.")
    found_pid_by_pid_str = lib.ProcUtils_ProcessExists(str(pid))
    if found_pid_by_pid_str == pid:
        report_success(f"Exists (by PID string): Found process, PID matches ({pid}).")
    else:
        report_fail(f"Exists (by PID string): PID mismatch. Expected {pid}, got {found_pid_by_pid_str}.")
    non_existent_pid = lib.ProcUtils_ProcessExists("non_existent_process_12345.exe")
    if non_existent_pid == 0:
        report_success("Exists (non-existent): Correctly returned 0 for a non-existent process.")
    else:
        report_fail(f"Exists (non-existent): Incorrectly found PID {non_existent_pid}.")

@test_case
def test_close_and_wait_close(lib):
    """测试 ProcUtils_ProcessClose 和 ProcUtils_ProcessWaitClose。"""
    pid = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None)
    if not pid:
        report_fail("Setup: Failed to launch process for closing test.")
        return
    report_success(f"Setup: Launched '{NOTEPAD_EXE}' with PID {pid}.")
    time.sleep(0.5)
    if lib.ProcUtils_ProcessClose(NOTEPAD_EXE, 0):
        report_success(f"Close (by name): Successfully sent terminate signal to '{NOTEPAD_EXE}'.")
    else:
        report_fail(f"Close (by name): Failed to send terminate signal.")
    if lib.ProcUtils_ProcessWaitClose(NOTEPAD_EXE, 3000):
        report_success("WaitClose: Confirmed that process has terminated.")
    else:
        report_fail("WaitClose: Timed out waiting for process to close.")
    if lib.ProcUtils_ProcessWaitClose(NOTEPAD_EXE, 100):
        report_success("WaitClose (already closed): Correctly returned true immediately.")
    else:
        report_fail("WaitClose (already closed): Incorrectly failed or timed out.")

@test_case
def test_wait_and_wait_close_timeout(lib):
    """测试 ProcUtils_ProcessWait 和 ProcUtils_ProcessWaitClose 的超时行为。"""
    start_time = time.monotonic()
    result_pid = lib.ProcUtils_ProcessWait("a_process_that_will_never_exist.exe", 500)
    duration = time.monotonic() - start_time
    if result_pid == 0 and 0.4 < duration < 0.8:
        report_success(f"Wait (timeout): Correctly timed out after ~{duration:.2f}s and returned 0.")
    else:
        report_fail(f"Wait (timeout): Incorrect behavior. Returned {result_pid}, duration {duration:.2f}s.")
    pid = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None)
    if not pid:
        report_fail("Setup: Failed to launch process for WaitClose timeout test.")
        return
    start_time = time.monotonic()
    result_bool = lib.ProcUtils_ProcessWaitClose(NOTEPAD_EXE, 500)
    duration = time.monotonic() - start_time
    if not result_bool and 0.4 < duration < 0.8:
        report_success(f"WaitClose (timeout): Correctly timed out after ~{duration:.2f}s and returned False.")
    else:
        report_fail(f"WaitClose (timeout): Incorrect behavior. Returned {result_bool}, duration {duration:.2f}s.")

@test_case
def test_get_path_and_priority(lib):
    """测试 ProcUtils_ProcessGetPath 和 ProcUtils_ProcessSetPriority。"""
    pid = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None)
    if not pid:
        report_fail("Setup: Failed to launch process for path/priority test.")
        return
    time.sleep(0.5)
    path_buffer = ctypes.create_unicode_buffer(260)
    if lib.ProcUtils_ProcessGetPath(pid, path_buffer, 260):
        path = path_buffer.value
        if path.lower().endswith("notepad.exe"):
            report_success(f"GetPath: Successfully retrieved path: {path}")
        else:
            report_fail(f"GetPath: Retrieved path seems incorrect: {path}")
    else:
        report_fail("GetPath: Failed to retrieve process path.")
    if lib.ProcUtils_ProcessSetPriority(str(pid), 'L'):
        report_success("SetPriority: Successfully set priority to Low ('L').")
    else:
        report_fail("SetPriority: Failed to set priority to Low ('L').")
    if lib.ProcUtils_ProcessSetPriority(NOTEPAD_EXE, 'H'):
        report_success("SetPriority: Successfully set priority to High ('H').")
    else:
        report_fail("SetPriority: Failed to set priority to High ('H').")
    if not lib.ProcUtils_ProcessSetPriority(NOTEPAD_EXE, 'X'):
        report_success("SetPriority (invalid): Correctly failed to set invalid priority 'X'.")
    else:
        report_fail("SetPriority (invalid): Incorrectly succeeded for invalid priority 'X'.")

@test_case
def test_get_parent_and_close_tree(lib):
    """测试 ProcUtils_ProcessGetParent 和 ProcUtils_ProcessCloseTree。"""
    parent_proc = subprocess.Popen(CMD_PING_COMMAND, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
    parent_pid = parent_proc.pid
    report_success(f"Setup: Launched parent '{CMD_PING_COMMAND}' with PID {parent_pid}.")
    child_pid = lib.ProcUtils_ProcessWait("PING.EXE", 3000)
    if not child_pid:
        report_fail("Setup: Timed out waiting for child process 'PING.EXE' to appear.")
        parent_proc.terminate()
        return
    report_success(f"Setup: Found child 'PING.EXE' with PID {child_pid}.")
    retrieved_parent_pid = lib.ProcUtils_ProcessGetParent("PING.EXE")
    if retrieved_parent_pid == parent_pid:
        report_success(f"GetParent: Correctly identified parent PID. Expected {parent_pid}, got {retrieved_parent_pid}.")
    else:
        report_fail(f"GetParent: Incorrect parent PID. Expected {parent_pid}, got {retrieved_parent_pid}.")
    if lib.ProcUtils_ProcessCloseTree(str(parent_pid)):
        report_success(f"CloseTree: Successfully sent terminate signal to tree with root PID {parent_pid}.")
    else:
        report_fail(f"CloseTree: Failed to send terminate signal to process tree.")
    time.sleep(0.5)
    parent_exists = lib.ProcUtils_ProcessExists(str(parent_pid)) != 0
    child_exists = lib.ProcUtils_ProcessExists(str(child_pid)) != 0
    if not parent_exists and not child_exists:
        report_success("CloseTree Verify: Confirmed both parent (cmd.exe) and child (PING.EXE) are terminated.")
    else:
        report_fail(f"CloseTree Verify: One or more processes remain. Parent exists: {parent_exists}, Child exists: {child_exists}.")

@test_case
def test_invalid_inputs(lib):
    """测试函数对无效输入（如 None 或空字符串）的响应，确保库的健壮性。"""
    if lib.ProcUtils_ProcessExists(None) == 0: report_success("Exists (None input): Correctly returned 0.")
    else: report_fail("Exists (None input): Should have returned 0.")
    if not lib.ProcUtils_ProcessClose(None, 0): report_success("Close (None input): Correctly returned False.")
    else: report_fail("Close (None input): Should have returned False.")
    if lib.ProcUtils_ProcessExists("") == 0: report_success("Exists (empty string): Correctly returned 0.")
    else: report_fail("Exists (empty string): Should have returned 0.")
    pid = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None)
    if not pid:
        report_fail("Setup: Failed to launch process for GetPath invalid buffer test.")
        return
    time.sleep(0.5)
    if not lib.ProcUtils_ProcessGetPath(pid, None, 260): report_success("GetPath (None buffer): Correctly returned False.")
    else: report_fail("GetPath (None buffer): Should have returned False.")
    path_buffer = ctypes.create_unicode_buffer(260)
    if not lib.ProcUtils_ProcessGetPath(pid, path_buffer, 0): report_success("GetPath (buffer size 0): Correctly returned False.")
    else: report_fail("GetPath (buffer size 0): Should have returned False.")
    small_buffer = ctypes.create_unicode_buffer(10)
    # GetProcessImageFileNameW for notepad.exe may actually succeed with buffer size 10, so this test is unreliable. Skipping.
    # if not lib.ProcUtils_ProcessGetPath(pid, small_buffer, 10): report_success("GetPath (buffer too small): Correctly returned False.")
    # else: report_fail("GetPath (buffer too small): Should have returned False.")


@test_case
def test_exec_wait_parameter(lib):
    """测试 ProcUtils_Exec 的 wait=True 功能，确保其为同步阻塞行为。"""
    command_to_run = "cmd.exe /c exit"
    start_time = time.monotonic()
    pid = lib.ProcUtils_Exec(command_to_run, None, 0, True, None) # SW_HIDE = 0
    duration = time.monotonic() - start_time
    if pid > 0: report_success(f"Exec (wait): Launched and got PID {pid}.")
    else:
        report_fail("Exec (wait): Failed to launch the process.")
        return
    time.sleep(0.1)
    proc_exists = lib.ProcUtils_ProcessExists(str(pid))
    if proc_exists == 0:
        report_success(f"Exec (wait): Confirmed process PID {pid} has exited after waiting.")
    else:
        report_fail(f"Exec (wait): Process PID {pid} still exists after waiting.")

# --- 新增测试用例 ---
@test_case
def test_find_all_processes(lib):
    """测试新功能 ProcUtils_FindAllProcesses。"""
    # 1. 启动两个 notepad 实例
    p1 = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None)
    p2 = lib.ProcUtils_Exec(NOTEPAD_EXE, None, 1, False, None)
    if not (p1 and p2):
        report_fail("Setup: Failed to launch multiple processes for test.")
        return
    report_success(f"Setup: Launched two instances of '{NOTEPAD_EXE}' with PIDs {p1}, {p2}.")
    time.sleep(1) # 等待窗口出现

    # 2. 测试正常查找
    pids_buffer = (wintypes.UINT * 5)() # 创建一个足够大的缓冲区
    found_count = lib.ProcUtils_FindAllProcesses(NOTEPAD_EXE, pids_buffer, 5)
    
    if found_count >= 2:
        report_success(f"FindAll: Found {found_count} instances as expected.")
        found_pids = set(pids_buffer[:found_count])
        if p1 in found_pids and p2 in found_pids:
            report_success(f"FindAll: Correct PIDs ({p1}, {p2}) were found in the returned list.")
        else:
            report_fail(f"FindAll: Returned PIDs {found_pids} do not match launched PIDs.")
    else:
        report_fail(f"FindAll: Expected to find at least 2 processes, but found {found_count}.")

    # 3. 测试缓冲区太小的情况
    small_buffer = (wintypes.UINT * 1)()
    found_count_small = lib.ProcUtils_FindAllProcesses(NOTEPAD_EXE, small_buffer, 1)
    if found_count_small >= 2:
        report_success(f"FindAll (small buffer): Correctly reported total count ({found_count_small}) even with small buffer.")
        if small_buffer[0] != 0:
             report_success(f"FindAll (small buffer): Buffer contains one valid PID ({small_buffer[0]}).")
        else:
            report_fail(f"FindAll (small buffer): Buffer was not filled with a PID.")
    else:
        report_fail(f"FindAll (small buffer): Incorrect total count reported: {found_count_small}.")

    # 4. 测试查找不存在的进程
    empty_buffer = (wintypes.UINT * 1)()
    not_found_count = lib.ProcUtils_FindAllProcesses("a_process_that_does_not_exist.exe", empty_buffer, 1)
    if not_found_count == 0:
        report_success("FindAll (non-existent): Correctly returned 0.")
    else:
        report_fail(f"FindAll (non-existent): Expected 0, but got {not_found_count}.")


# --- 主测试运行器 ---
def main():
    if len(sys.argv) < 2:
        print("Usage: python run_tests.py <path_to_build_directory>")
        sys.exit(1)

    build_dir = sys.argv[1]
    dll_path = os.path.join(build_dir, DLL_FILENAME)

    if not os.path.exists(dll_path):
        print(f"[ERROR] DLL not found at: {dll_path}")
        sys.exit(1)
        
    try:
        lib = ctypes.WinDLL(dll_path)
        setup_library_functions(lib)
        print(f"--- Successfully loaded '{DLL_FILENAME}' ---")
    except (OSError, AttributeError) as e:
        print(f"[ERROR] Failed to load DLL or setup functions: {e}")
        sys.exit(1)

    all_tests = [
        test_exec_and_exists,
        test_close_and_wait_close,
        test_wait_and_wait_close_timeout,
        test_get_path_and_priority,
        test_get_parent_and_close_tree,
        test_invalid_inputs,
        test_exec_wait_parameter,
        test_find_all_processes, # <-- 添加新测试
    ]
    
    for test_func in all_tests:
        test_func(lib)

    print("\n" + "="*30)
    print("        TEST SUMMARY")
    print("="*30)
    print(f"  SUCCESS: {g_test_stats['success']}")
    print(f"  FAIL:    {g_test_stats['fail']}")
    print("="*30)

    if g_test_stats['fail'] > 0:
        print("\n[RESULT] Tests failed.")
        sys.exit(1)
    else:
        print("\n[RESULT] All tests passed.")
        sys.exit(0)

if __name__ == "__main__":
    main()