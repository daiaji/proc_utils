import ctypes
import os
import sys
import time
import subprocess
from ctypes import wintypes

# --- 全局配置 ---
DLL_FILENAME = "proc_utils.dll"
NOTEPAD_EXE = "notepad.exe"
# 一个稳定的、可以作为子进程的目标 (ping会持续一段时间)
CMD_PING_COMMAND = f'cmd.exe /c "ping -n 20 127.0.0.1 > NUL"'

# WinAPI 常量
PROCESS_ALL_ACCESS = 0x1F0FFF
SYNCHRONIZE = 0x00100000
SW_HIDE = 0
SW_SHOW = 5

g_test_stats = {"success": 0, "fail": 0}


# --- Ctypes 结构体定义 ---
class ProcUtils_ProcessInfo(ctypes.Structure):
    _fields_ = [
        ("pid", wintypes.DWORD),
        ("parent_pid", wintypes.DWORD),
        ("session_id", wintypes.DWORD),
        ("exe_path", wintypes.WCHAR * 260),
        ("command_line", wintypes.WCHAR * 2048),
        ("memory_usage_bytes", ctypes.c_ulonglong),
        ("thread_count", wintypes.DWORD),
    ]


class ProcUtils_ProcessResult(ctypes.Structure):
    _fields_ = [
        ("pid", wintypes.DWORD),
        ("process_handle", wintypes.HANDLE),
        ("last_error_code", wintypes.DWORD),
    ]


# --- 测试装饰器和助手函数 ---
def test_case(func):
    """一个简单的装饰器，用于运行测试函数、打印结果并进行清理。"""

    def wrapper(lib, kernel32):
        test_name = func.__name__
        print(f"\n--- Running Test: {test_name} ---")
        # 确保测试前环境干净
        cleanup_process(NOTEPAD_EXE)
        cleanup_process("cmd.exe")
        cleanup_process("PING.EXE")
        time.sleep(0.2)  # 短暂等待清理生效

        try:
            func(lib, kernel32)
        except Exception as e:
            import traceback

            traceback.print_exc()
            report_fail(f"Test crashed with an exception: {e}")
        finally:
            # 确保测试后环境干净
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
    """强制关闭所有指定名称的进程，用于测试环境清理。"""
    subprocess.run(
        f"taskkill /F /IM {process_name}",
        check=False,
        capture_output=True,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )


def setup_library_functions(lib):
    """为库中的所有函数定义参数类型和返回类型。"""
    # 模块 1：查找与枚举
    lib.ProcUtils_OpenProcessByPid.argtypes = [wintypes.DWORD, wintypes.DWORD]
    lib.ProcUtils_OpenProcessByPid.restype = wintypes.HANDLE
    lib.ProcUtils_OpenProcessByName.argtypes = [wintypes.LPCWSTR, wintypes.DWORD]
    lib.ProcUtils_OpenProcessByName.restype = wintypes.HANDLE
    lib.ProcUtils_FindAllProcesses.argtypes = [
        wintypes.LPCWSTR,
        ctypes.POINTER(wintypes.DWORD),
        wintypes.INT,
    ]
    lib.ProcUtils_FindAllProcesses.restype = wintypes.INT
    lib.ProcUtils_ProcessExists.argtypes = [wintypes.LPCWSTR]
    lib.ProcUtils_ProcessExists.restype = wintypes.DWORD

    # 模块 2：创建与执行
    lib.ProcUtils_CreateProcess.argtypes = [
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.INT,
        wintypes.LPCWSTR,
    ]
    lib.ProcUtils_CreateProcess.restype = ProcUtils_ProcessResult
    lib.ProcUtils_LaunchProcess.argtypes = [
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.INT,
        wintypes.LPCWSTR,
    ]
    lib.ProcUtils_LaunchProcess.restype = wintypes.DWORD
    lib.ProcUtils_CreateProcessAsSystem.argtypes = [
        wintypes.LPCWSTR,
        wintypes.LPCWSTR,
        wintypes.INT,
    ]
    lib.ProcUtils_CreateProcessAsSystem.restype = ProcUtils_ProcessResult

    # 模块 3：信息获取
    lib.ProcUtils_ProcessGetInfo.argtypes = [
        wintypes.DWORD,
        ctypes.POINTER(ProcUtils_ProcessInfo),
    ]
    lib.ProcUtils_ProcessGetInfo.restype = ctypes.c_bool
    lib.ProcUtils_ProcessGetCommandLine.argtypes = [
        wintypes.DWORD,
        wintypes.LPWSTR,
        wintypes.INT,
    ]
    lib.ProcUtils_ProcessGetCommandLine.restype = ctypes.c_bool
    lib.ProcUtils_ProcessGetPath.argtypes = [
        wintypes.DWORD,
        wintypes.LPWSTR,
        wintypes.INT,
    ]
    lib.ProcUtils_ProcessGetPath.restype = ctypes.c_bool
    lib.ProcUtils_ProcessGetParent.argtypes = [wintypes.LPCWSTR]
    lib.ProcUtils_ProcessGetParent.restype = wintypes.DWORD

    # 模块 4：控制与交互
    lib.ProcUtils_ProcessClose.argtypes = [wintypes.LPCWSTR, wintypes.UINT]
    lib.ProcUtils_ProcessClose.restype = ctypes.c_bool
    lib.ProcUtils_TerminateProcessByPid.argtypes = [wintypes.DWORD, wintypes.UINT]
    lib.ProcUtils_TerminateProcessByPid.restype = ctypes.c_bool
    lib.ProcUtils_ProcessCloseTree.argtypes = [wintypes.LPCWSTR]
    lib.ProcUtils_ProcessCloseTree.restype = ctypes.c_bool
    lib.ProcUtils_TerminateProcessTreeByPid.argtypes = [wintypes.DWORD]
    lib.ProcUtils_TerminateProcessTreeByPid.restype = ctypes.c_bool
    lib.ProcUtils_ProcessSetPriority.argtypes = [wintypes.LPCWSTR, wintypes.WCHAR]
    lib.ProcUtils_ProcessSetPriority.restype = ctypes.c_bool
    lib.ProcUtils_ProcessWait.argtypes = [wintypes.LPCWSTR, wintypes.INT]
    lib.ProcUtils_ProcessWait.restype = wintypes.DWORD
    lib.ProcUtils_ProcessWaitClose.argtypes = [wintypes.LPCWSTR, wintypes.INT]
    lib.ProcUtils_ProcessWaitClose.restype = ctypes.c_bool
    lib.ProcUtils_WaitForProcessExit.argtypes = [wintypes.HANDLE, wintypes.INT]
    lib.ProcUtils_WaitForProcessExit.restype = ctypes.c_bool


# --- 测试用例 ---


@test_case
def test_launch_and_exists(lib, kernel32):
    """测试 LaunchProcess 和 ProcessExists 的多种形式。"""
    pid = lib.ProcUtils_LaunchProcess(NOTEPAD_EXE, None, SW_SHOW, None)
    if not pid > 0:
        report_fail(f"Launch: Failed to launch '{NOTEPAD_EXE}'.")
        return
    report_success(f"Launch: Successfully launched '{NOTEPAD_EXE}' with PID {pid}.")
    time.sleep(0.5)

    found_pid_by_name = lib.ProcUtils_ProcessExists(NOTEPAD_EXE)
    if found_pid_by_name == pid:
        report_success(f"Exists (by name): Found process, PID matches ({pid}).")
    else:
        report_fail(
            f"Exists (by name): PID mismatch. Expected ~{pid}, got {found_pid_by_name}."
        )

    found_pid_by_pid_str = lib.ProcUtils_ProcessExists(str(pid))
    if found_pid_by_pid_str == pid:
        report_success(f"Exists (by PID string): Found process, PID matches ({pid}).")
    else:
        report_fail(
            f"Exists (by PID string): PID mismatch. Expected {pid}, got {found_pid_by_pid_str}."
        )

    non_existent_pid = lib.ProcUtils_ProcessExists("non_existent_process_12345.exe")
    if non_existent_pid == 0:
        report_success(
            "Exists (non-existent): Correctly returned 0 for a non-existent process."
        )
    else:
        report_fail(f"Exists (non-existent): Incorrectly found PID {non_existent_pid}.")


@test_case
def test_create_and_open_process(lib, kernel32):
    """测试 CreateProcess, OpenProcessByPid/Name, 和基于句柄的等待。"""
    result = lib.ProcUtils_CreateProcess(NOTEPAD_EXE, None, SW_SHOW, None)
    handle_to_close = result.process_handle
    try:
        if not (result.pid > 0 and result.process_handle):
            report_fail(
                f"CreateProcess: Failed. PID={result.pid}, Handle={result.process_handle}, Error={result.last_error_code}"
            )
            return
        report_success(
            f"CreateProcess: Returned valid PID {result.pid} and Handle {result.process_handle}."
        )

        h_by_pid = lib.ProcUtils_OpenProcessByPid(result.pid, SYNCHRONIZE)
        if h_by_pid:
            report_success("OpenByPid: Successfully opened handle for the new process.")
            kernel32.CloseHandle(h_by_pid)
        else:
            report_fail("OpenByPid: Failed to open handle for the new process.")

        if not lib.ProcUtils_WaitForProcessExit(result.process_handle, 200):
            report_success(f"WaitForProcessExit (timeout): Correctly timed out.")
        else:
            report_fail(f"WaitForProcessExit (timeout): Incorrectly returned true.")

        lib.ProcUtils_TerminateProcessByPid(result.pid, 0)

        if lib.ProcUtils_WaitForProcessExit(result.process_handle, 500):
            report_success(
                "WaitForProcessExit (terminated): Correctly detected process exit."
            )
        else:
            report_fail(
                "WaitForProcessExit (terminated): Failed to detect process exit."
            )

    finally:
        if handle_to_close:
            kernel32.CloseHandle(handle_to_close)


@test_case
def test_terminate_and_wait_close(lib, kernel32):
    """测试 TerminateProcessByPid 和 ProcessWaitClose。"""
    pid = lib.ProcUtils_LaunchProcess(NOTEPAD_EXE, None, SW_SHOW, None)
    if not pid:
        report_fail("Setup: Failed to launch process for closing test.")
        return
    report_success(f"Setup: Launched '{NOTEPAD_EXE}' with PID {pid}.")
    time.sleep(0.5)

    if lib.ProcUtils_TerminateProcessByPid(pid, 0):
        report_success(
            f"TerminateByPid: Successfully sent terminate signal to PID {pid}."
        )
    else:
        report_fail(f"TerminateByPid: Failed to send terminate signal.")

    if lib.ProcUtils_ProcessWaitClose(NOTEPAD_EXE, 3000):
        report_success("WaitClose: Confirmed that process has terminated.")
    else:
        report_fail("WaitClose: Timed out waiting for process to close.")


@test_case
def test_wait_and_wait_close_timeout(lib, kernel32):
    """测试 ProcessWait 和 ProcessWaitClose 的超时行为。"""
    start_time = time.monotonic()
    result_pid = lib.ProcUtils_ProcessWait("a_process_that_will_never_exist.exe", 500)
    duration = time.monotonic() - start_time
    if result_pid == 0 and 0.4 < duration < 0.8:
        report_success(
            f"Wait (timeout): Correctly timed out after ~{duration:.2f}s and returned 0."
        )
    else:
        report_fail(
            f"Wait (timeout): Incorrect behavior. Returned {result_pid}, duration {duration:.2f}s."
        )

    pid = lib.ProcUtils_LaunchProcess(NOTEPAD_EXE, None, SW_SHOW, None)
    if not pid:
        report_fail("Setup: Failed to launch process for WaitClose timeout test.")
        return

    start_time = time.monotonic()
    result_bool = lib.ProcUtils_ProcessWaitClose(NOTEPAD_EXE, 500)
    duration = time.monotonic() - start_time
    if not result_bool and 0.4 < duration < 0.8:
        report_success(
            f"WaitClose (timeout): Correctly timed out after ~{duration:.2f}s and returned False."
        )
    else:
        report_fail(
            f"WaitClose (timeout): Incorrect behavior. Returned {result_bool}, duration {duration:.2f}s."
        )


@test_case
def test_get_path_and_set_priority(lib, kernel32):
    """测试 ProcessGetPath 和 ProcessSetPriority。"""
    pid = lib.ProcUtils_LaunchProcess(NOTEPAD_EXE, None, SW_SHOW, None)
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

    if lib.ProcUtils_ProcessSetPriority(str(pid), "L"):
        report_success("SetPriority: Successfully set priority to Low ('L').")
    else:
        report_fail("SetPriority: Failed to set priority to Low ('L').")

    if not lib.ProcUtils_ProcessSetPriority(NOTEPAD_EXE, "X"):
        report_success(
            "SetPriority (invalid): Correctly failed to set invalid priority 'X'."
        )
    else:
        report_fail(
            "SetPriority (invalid): Incorrectly succeeded for invalid priority 'X'."
        )


@test_case
def test_get_parent_and_close_tree(lib, kernel32):
    """测试 ProcessGetParent 和 ProcessCloseTree/TerminateProcessTreeByPid。"""
    parent_proc = subprocess.Popen(
        CMD_PING_COMMAND, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP
    )
    parent_pid = parent_proc.pid
    report_success(
        f"Setup: Launched parent '{CMD_PING_COMMAND}' with PID {parent_pid}."
    )

    child_pid = lib.ProcUtils_ProcessWait("PING.EXE", 3000)
    if not child_pid:
        report_fail("Setup: Timed out waiting for child process 'PING.EXE' to appear.")
        parent_proc.terminate()
        return
    report_success(f"Setup: Found child 'PING.EXE' with PID {child_pid}.")

    retrieved_parent_pid = lib.ProcUtils_ProcessGetParent("PING.EXE")
    if retrieved_parent_pid == parent_pid:
        report_success(f"GetParent: Correctly identified parent PID as {parent_pid}.")
    else:
        report_fail(
            f"GetParent: Incorrect parent PID. Expected {parent_pid}, got {retrieved_parent_pid}."
        )

    if lib.ProcUtils_TerminateProcessTreeByPid(parent_pid):
        report_success(
            f"TerminateProcessTreeByPid: Successfully sent signal to tree root PID {parent_pid}."
        )
    else:
        report_fail(
            f"TerminateProcessTreeByPid: Failed to send signal to process tree."
        )

    time.sleep(0.5)
    if (
        lib.ProcUtils_ProcessExists(str(parent_pid)) == 0
        and lib.ProcUtils_ProcessExists(str(child_pid)) == 0
    ):
        report_success(
            "CloseTree Verify: Confirmed both parent and child are terminated."
        )
    else:
        report_fail("CloseTree Verify: One or more processes remain.")


@test_case
def test_find_all_processes(lib, kernel32):
    """测试 ProcUtils_FindAllProcesses。"""
    p1 = lib.ProcUtils_LaunchProcess(NOTEPAD_EXE, None, SW_HIDE, None)
    p2 = lib.ProcUtils_LaunchProcess(NOTEPAD_EXE, None, SW_HIDE, None)
    if not (p1 and p2):
        report_fail("Setup: Failed to launch multiple processes for test.")
        return
    report_success(
        f"Setup: Launched two instances of '{NOTEPAD_EXE}' with PIDs {p1}, {p2}."
    )
    time.sleep(1)

    found_count_only = lib.ProcUtils_FindAllProcesses(NOTEPAD_EXE, None, 0)
    if found_count_only >= 2:
        report_success(
            f"FindAll (count only): Found {found_count_only} instances as expected."
        )
    else:
        report_fail(
            f"FindAll (count only): Expected at least 2, but got {found_count_only}."
        )
        return

    pids_buffer = (wintypes.DWORD * found_count_only)()
    stored_count = lib.ProcUtils_FindAllProcesses(
        NOTEPAD_EXE, pids_buffer, found_count_only
    )
    if stored_count == found_count_only:
        report_success(f"FindAll (get data): Stored {stored_count} PIDs.")
        found_pids = set(pids_buffer[:stored_count])
        if p1 in found_pids and p2 in found_pids:
            report_success(
                f"FindAll (verify): Correct PIDs ({p1}, {p2}) were found in the list."
            )
        else:
            report_fail(
                f"FindAll (verify): Returned PIDs {found_pids} do not match launched PIDs."
            )
    else:
        report_fail(
            f"FindAll (get data): Count mismatch. Expected {found_count_only}, stored {stored_count}."
        )


@test_case
def test_get_full_process_info(lib, kernel32):
    """测试增强的 GetProcessInfo 和独立的 GetCommandLine。"""
    cmd_line = f"{NOTEPAD_EXE} C:\\testfile.txt"
    pid = lib.ProcUtils_LaunchProcess(cmd_line, None, SW_HIDE, None)
    if not pid:
        report_fail("Setup: Failed to launch process for info test.")
        return
    report_success(f"Setup: Launched '{cmd_line}' with PID {pid}.")
    time.sleep(0.5)

    info = ProcUtils_ProcessInfo()
    if lib.ProcUtils_ProcessGetInfo(pid, ctypes.byref(info)):
        report_success("GetInfo: Function call succeeded.")
        if info.pid == pid:
            report_success(f"  - PID: OK ({info.pid})")
        else:
            report_fail(f"  - PID: Mismatch, got {info.pid}")

        if info.exe_path.lower().endswith("notepad.exe"):
            report_success(f"  - Path: OK ('{info.exe_path}')")
        else:
            report_fail(f"  - Path: Incorrect ('{info.exe_path}')")

        if (
            "notepad.exe" in info.command_line.lower()
            and "testfile.txt" in info.command_line.lower()
        ):
            report_success(f"  - CommandLine: OK ('{info.command_line}')")
        else:
            report_fail(
                f"  - CommandLine: Incorrect or missing args ('{info.command_line}')"
            )

        if info.session_id > 0:
            report_success(f"  - Session ID: OK ({info.session_id})")
        else:
            report_fail(f"  - Session ID: Invalid ({info.session_id})")
    else:
        report_fail("GetInfo: Function call failed.")

    if not lib.ProcUtils_ProcessGetInfo(999999, ctypes.byref(info)):
        report_success(
            "GetInfo (non-existent): Correctly failed for a non-existent PID."
        )
    else:
        report_fail(
            "GetInfo (non-existent): Incorrectly succeeded for a non-existent PID."
        )


@test_case
def test_invalid_inputs(lib, kernel32):
    """测试函数对无效输入（如 None 或空字符串）的响应。"""
    if lib.ProcUtils_ProcessExists(None) == 0:
        report_success("Exists (None input): Correctly returned 0.")
    else:
        report_fail("Exists (None input): Should have returned 0.")

    if not lib.ProcUtils_ProcessClose(None, 0):
        report_success("Close (None input): Correctly returned False.")
    else:
        report_fail("Close (None input): Should have returned False.")

    if lib.ProcUtils_ProcessExists("") == 0:
        report_success("Exists (empty string): Correctly returned 0.")
    else:
        report_fail("Exists (empty string): Should have returned 0.")

    if not lib.ProcUtils_ProcessGetPath(99999, None, 260):
        report_success("GetPath (None buffer): Correctly returned False.")
    else:
        report_fail("GetPath (None buffer): Should have returned False.")


@test_case
def test_create_as_system(lib, kernel32):
    """测试 CreateProcessAsSystem (需要管理员权限)。"""
    print("Info: This test requires administrative privileges to succeed.")
    cmd = "whoami.exe"
    result = lib.ProcUtils_CreateProcessAsSystem(cmd, None, SW_HIDE)
    handle_to_close = result.process_handle
    try:
        if result.pid > 0 and result.process_handle:
            # 如果成功，说明是在管理员权限下运行的本地测试
            report_success(
                f"CreateAsSystem: Successfully created process '{cmd}' with PID {result.pid}."
            )
            time.sleep(1)  # 给它时间退出
            if lib.ProcUtils_ProcessExists(str(result.pid)) == 0:
                report_success(
                    "CreateAsSystem (verify): Process has exited as expected."
                )
            else:
                print(
                    f"Warning: System process {result.pid} still running, terminating manually."
                )
                lib.ProcUtils_TerminateProcessByPid(result.pid, 0)
        else:
            # 如果失败，直接从返回的结构体中读取错误码
            last_error = result.last_error_code

            # 常见的权限/环境错误代码：5 (拒绝访问), 1314 (无特权), 1008 (无令牌), 6 (句柄无效)
            expected_ci_errors = [5, 6, 1008, 1314]
            if last_error in expected_ci_errors:
                report_success(
                    f"CreateAsSystem: Failed as expected without admin rights/interactive session. Error: {last_error}"
                )
            elif last_error == 0:
                report_fail(
                    f"CreateAsSystem: Failed but the returned error code was 0, which is incorrect."
                )
            else:
                report_fail(
                    f"CreateAsSystem: Failed with an unexpected error code: {last_error}"
                )
    finally:
        if handle_to_close:
            kernel32.CloseHandle(handle_to_close)


# --- 主测试运行器 ---
def main():
    if len(sys.argv) < 2:
        print("Usage: python run_tests.py <path_to_build_directory>")
        sys.exit(1)

    build_dir = sys.argv[1]
    dll_path_release = os.path.join(build_dir, "Release", DLL_FILENAME)
    dll_path_debug = os.path.join(build_dir, "Debug", DLL_FILENAME)
    dll_path_plain = os.path.join(build_dir, DLL_FILENAME)

    if os.path.exists(dll_path_release):
        dll_path = dll_path_release
    elif os.path.exists(dll_path_debug):
        dll_path = dll_path_debug
    elif os.path.exists(dll_path_plain):
        dll_path = dll_path_plain
    else:
        print(
            f"[ERROR] DLL not found in common build output locations within: {build_dir}"
        )
        sys.exit(1)

    try:
        lib = ctypes.WinDLL(dll_path)
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        setup_library_functions(lib)
        print(f"--- Successfully loaded '{dll_path}' ---")
    except (OSError, AttributeError) as e:
        print(f"[ERROR] Failed to load DLL or setup functions: {e}")
        sys.exit(1)

    all_tests = [
        test_launch_and_exists,
        test_create_and_open_process,
        test_terminate_and_wait_close,
        test_wait_and_wait_close_timeout,
        test_get_path_and_set_priority,
        test_get_parent_and_close_tree,
        test_find_all_processes,
        test_get_full_process_info,
        test_invalid_inputs,
        test_create_as_system,
    ]

    for test_func in all_tests:
        test_func(lib, kernel32)

    print("\n" + "=" * 30)
    print("        TEST SUMMARY")
    print("=" * 30)
    print(f"  SUCCESS: {g_test_stats['success']}")
    print(f"  FAIL:    {g_test_stats['fail']}")
    print("=" * 30)

    if g_test_stats["fail"] > 0:
        print("\n[RESULT] Tests failed.")
        sys.exit(1)
    else:
        print("\n[RESULT] All tests passed.")
        sys.exit(0)


if __name__ == "__main__":
    main()
