# Proc-Utils: C++ Win32 进程管理工具库

`proc_utils` 是一个轻量级的、无外部依赖的工具库，为 Windows 平台提供了一套简洁、健壮的 C 语言接口和现代化的 C++ 封装，用于常见的进程管理任务。

该库旨在原子化和简化与进程相关的操作，如查找、创建、终止、等待以及获取详细信息，使其可以被 C/C++、Python、Lua (via FFI) 等多种语言轻松调用。

## ✨ 功能特性

-   **进程查找与枚举**:
    -   按 PID 或进程名打开进程并获取句柄 (`OpenProcessByPid`, `OpenProcessByName`)。
    -   检查进程是否存在 (`ProcessExists`)。
    -   查找所有同名进程的 PID 列表 (`FindAllProcesses`)。
-   **进程创建与执行**:
    -   `CreateProcess`: 创建新进程，并原子性地返回 PID 和进程句柄，用于高级操作。
    -   `LaunchProcess`: 以“发后不理”的方式启动新进程，只返回 PID，简单便捷。
    -   `CreateProcessAsSystem`: **【新】** 在当前活动桌面以 `SYSTEM` 权限创建进程。
-   **进程终止**:
    -   按 PID 强制终止指定进程 (`TerminateProcessByPid`)。
    -   终止一个完整的进程树，包括所有子进程 (`TerminateProcessTreeByPid`)。
-   **进程等待**:
    -   同步等待，直到指定名称的进程出现 (`ProcessWait`)。
    -   同步等待，直到指定进程结束 (`ProcessWaitClose`)。
    -   **【新】** 通过进程句柄高效等待进程结束 (`WaitForProcessExit`)。
-   **信息获取**:
    -   **【增强】** 获取进程的详细信息，包括 PID, 父 PID, 会话 ID, 内存使用, 线程数, 可执行文件路径以及**完整的命令行参数** (`ProcessGetInfo`)。
    -   单独获取进程的父进程 PID 或命令行 (`ProcessGetParent`, `ProcessGetCommandLine`)。
-   **属性修改**: 设置进程的CPU优先级 (`ProcessSetPriority`)。
-   **纯 C 接口**: 所有导出的核心函数均为 `extern "C"`，确保了跨语言调用的兼容性。
-   **现代 C++ 封装**:
    -   提供一个头文件级别的 C++ `ProcUtils::Process` 类。
    -   使用 **RAII** 自动管理进程句柄，杜绝资源泄漏。
    -   使用**移动语义**确保资源所有权的唯一性。
    -   通过 `std::optional` 和 `std::vector` 提供类型安全、现代化的接口。
-   **健壮的错误处理**: 所有 C 接口在失败时都会设置标准的 Win32 错误码，可通过 `GetLastError()` 获取。

## 🚀 如何使用

### 在 C/C++ 中使用

您需要 `proc_utils.dll` (或 `.lib`) 和 `proc_utils.h` 这两个文件。

1.  将 `proc_utils.h` 包含到您的源代码中。
2.  链接 `proc_utils.lib`。
3.  确保 `proc_utils.dll` 在运行时可被找到。

**C 语言示例**:

```c
#include "proc_utils.h"
#include <stdio.h>
#include <windows.h>

#pragma comment(lib, "proc_utils.lib")

int main() {
    const wchar_t* process_name = L"notepad.exe";
    
    // 场景: 创建进程并等待其结束
    ProcUtils_ProcessResult result = ProcUtils_CreateProcess(process_name, NULL, SW_SHOW, NULL);
    if (result.pid > 0) {
        printf("CreateProcess 成功, PID: %u, Handle: %p\n", result.pid, result.process_handle);

        printf("等待 3 秒或直到进程关闭...\n");
        // 使用新的基于句柄的等待函数，更高效
        ProcUtils_WaitForProcessExit(result.process_handle, 3000);
        printf("等待结束。\n");

        // **重要**: 必须手动关闭句柄！
        CloseHandle(result.process_handle);
    }
    
    return 0;
}
```

### 在 C++ 中使用 (推荐)

`proc_utils.h` 包含了一个现代、安全的 C++ 封装。

```cpp
#include "proc_utils.h"
#include <iostream>

#pragma comment(lib, "proc_utils.lib")

int main() {
    // 使用 C++ 封装，它通过 RAII 自动管理句柄的生命周期
    // 使用 std::optional 进行安全的错误处理
    if (auto notepad = ProcUtils::Process::exec(L"notepad.exe C:\\log.txt")) {
        std::wcout << L"成功启动 notepad.exe, PID: " << notepad->id() 
                   << L", Handle: " << notepad->handle() << std::endl;
        
        // 获取增强的进程信息
        if (auto info = notepad->get_info()) {
            std::wcout << L"  路径: " << info->exe_path << std::endl;
            std::wcout << L"  命令行: " << info->command_line << std::endl;
            std::wcout << L"  父进程ID: " << info->parent_pid << std::endl;
        }

        notepad->wait_for_exit(3000); // 等待最多3秒
    } // notepad 对象离开作用域时，其析构函数会自动调用 CloseHandle，无需手动管理
    else {
        std::wcerr << L"启动 notepad.exe 失败, 错误码: " << GetLastError() << std::endl;
    }
    
    std::wcout << L"notepad.exe 已关闭或其句柄已被自动释放。" << std::endl;
    
    return 0;
}
```

## 📜 API 参考

**重要**: 当函数返回 `false`, `0`, 或 `NULL` 表示失败时，可以立即调用 Windows API `GetLastError()` 来获取详细的错误代码。

| 函数名 | 描述 |
| :--- | :--- |
| **查找 & 枚举** | |
| `ProcUtils_OpenProcessByPid(pid, access)` | 按 PID 打开进程并返回 `HANDLE`。 |
| `ProcUtils_OpenProcessByName(name, access)` | 按名称查找并打开第一个匹配的进程，返回 `HANDLE`。|
| `ProcUtils_ProcessExists(name_or_pid)` | 检查进程是否存在，存在则返回其 PID，否则返回 0。 |
| `ProcUtils_FindAllProcesses(name, out_pids, size)`| 查找所有同名进程。返回找到的数量。|
| **创建 & 执行** | |
| `ProcUtils_CreateProcess(cmd, ...)` | 创建新进程，返回包含 PID 和 `HANDLE` 的结构体。|
| `ProcUtils_LaunchProcess(cmd, ...)` | 以“发后不理”模式启动进程，只返回 PID。|
| `ProcUtils_CreateProcessAsSystem(cmd, ...)` | **【新】** 以 `SYSTEM` 权限在当前桌面创建进程。|
| **信息获取** | |
| `ProcUtils_ProcessGetInfo(pid, out_info)` | **【增强】** 获取进程的详细信息（路径、命令行等）。|
| `ProcUtils_ProcessGetCommandLine(pid, ...)`| **【新】** 单独获取进程的完整命令行。|
| `ProcUtils_ProcessGetPath(pid, ...)` | 获取指定 PID 进程的完整路径。|
| `ProcUtils_ProcessGetParent(name_or_pid)`| 获取指定进程的父进程 ID。|
| **控制 & 交互** | |
| `ProcUtils_TerminateProcessByPid(pid, code)`| **【新】** 按 PID 终止进程。|
| `ProcUtils_TerminateProcessTreeByPid(pid)`| **【新】** 按 PID 终止一个进程及其所有子进程。|
| `ProcUtils_ProcessWait(name, timeout)` | 等待指定名称的进程出现。|
| `ProcUtils_WaitForProcessExit(handle, timeout)`| **【新】** 通过句柄等待一个进程结束，最高效。|
| `ProcUtils_ProcessSetPriority(...)` | 设置进程优先级 ('L', 'B', 'N', 'A', 'H', 'R')。|

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。