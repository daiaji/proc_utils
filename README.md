# Proc-Utils: C++ Win32 进程管理工具库

`proc_utils` 是一个轻量级的、无外部依赖的动态链接库 (DLL)，为 Windows 平台提供了一套简洁、易用的 C 语言接口，用于常见的进程管理任务。

该库旨在简化与进程相关的操作，如查找、启动、终止、等待以及修改进程属性，使其可以被 C/C++、Python、C# 等多种语言轻松调用。

## ✨ 功能特性

-   **进程查找**: 
    - 按进程名或 PID 检查进程是否存在。
    - 查找所有同名进程，并返回它们的 PID 列表。
-   **进程执行**: 
    - **【新】`CreateProcess`**: 创建新进程，并原子性返回 PID 和进程句柄，用于高级操作。
    - **【新】`LaunchProcess`**: 以“发后不理”的方式启动新进程，只返回 PID，简单便捷。
-   **进程终止**: 强制终止指定进程，或终止一个完整的进程树（包括所有子进程）。
-   **进程等待**:
    -   同步等待，直到指定名称的进程出现。
    -   同步等待，直到指定进程结束。
-   **信息获取**:
    -   获取进程可执行文件的完整路径。
    -   获取指定进程的父进程 PID。
    -   获取进程的详细信息 (PID, 父 PID, 路径, 内存使用, 线程数)。
-   **属性修改**: 设置进程的CPU优先级（如：低、正常、高、实时）。
-   **纯 C 接口**: 导出的函数均为 `extern "C"`，确保了跨语言调用的兼容性。
-   **可选的 C++ 封装**: 提供一个头文件级别的 C++ Wrapper，带来更现代化的编程体验，并自动管理资源。
-   **健壮的错误处理**: 所有 API 在失败时都会设置标准的 Win32 错误码，可通过 `GetLastError()` 获取。

## 🛠️ 如何编译

本项目使用 CMake 进行构建，需要一个支持 C++17 的编译器（例如 Visual Studio）。

### 编译环境要求
-   CMake (3.10 或更高版本)
-   Visual Studio 2017 或更高版本 (或其他支持 C++17 的 Windows 编译器)

### 编译步骤

1.  **克隆仓库**:
    ```bash
    git clone <your-repository-url>
    cd proc_utils
    ```

2.  **创建构建目录**:
    ```bash
    mkdir build
    cd build
    ```

3.  **生成项目文件 (以 Visual Studio 2022 64位为例)**:
    ```bash
    cmake .. -G "Visual Studio 17 2022" -A x64
    ```

4.  **编译项目**:
    -   **通过命令行**:
        ```bash
        cmake --build . --config Release
        ```
    -   **通过 Visual Studio**:
        用 Visual Studio 打开在 `build` 目录下生成的 `proc_utils.sln` 文件，然后选择 `Release` 配置并生成解决方案。

编译成功后，您将在 `build\Release` (或 `build\Debug`) 目录下找到 `proc_utils.dll` 和 `proc_utils.lib` 文件。

## 🚀 如何使用

### 在 C/C++ 中使用

您需要 `proc_utils.dll`、`proc_utils.lib` 和 `proc_utils.h` 这三个文件。

1.  将 `proc_utils.h` 包含到您的源代码中。
2.  将 `proc_utils.lib` 链接到您的项目。
3.  确保 `proc_utils.dll` 在程序运行时可以被找到（例如，与您的可执行文件放在同一目录）。

**C 语言示例**:

```c
#include "proc_utils.h"
#include <stdio.h>
#include <windows.h>

#pragma comment(lib, "proc_utils.lib")

int main() {
    const wchar_t* process_name = L"notepad.exe";
    
    // 场景1: 简单启动，不需要后续操作
    unsigned int pid = ProcUtils_LaunchProcess(process_name, NULL, SW_SHOW, NULL);
    if (pid > 0) {
        printf("成功启动 notepad.exe, PID: %u\n", pid);
        Sleep(1000);
        ProcUtils_ProcessClose(process_name, 0);
        printf("已关闭 notepad.exe\n");
    }

    // 场景2: 启动进程并需要后续高级操作
    ProcUtils_ProcessResult result = ProcUtils_CreateProcess(process_name, NULL, SW_SHOW, NULL);
    if (result.pid > 0) {
        printf("CreateProcess 成功, PID: %u, Handle: %p\n", result.pid, result.process_handle);

        // 使用句柄等待进程结束
        WaitForSingleObject(result.process_handle, 3000);

        // **重要**: 必须手动关闭句柄！
        CloseHandle(result.process_handle);
        printf("已关闭句柄并等待结束。\n");
    }
    
    return 0;
}
```

### 在 C++ 中使用 (推荐)

`proc_utils.h` 包含了一个可选的 C++ 封装，可以提供更现代和安全的编程体验。

```cpp
#include "proc_utils.h"
#include <iostream>
#include <windows.h>

#pragma comment(lib, "proc_utils.lib")

int main() {
    // 使用 C++ Wrapper，它会自动管理句柄的生命周期 (RAII)
    auto notepad = ProcUtils::Process::exec(L"notepad.exe", nullptr, SW_SHOW);
    if (notepad.is_valid()) {
        std::wcout << L"成功启动 notepad.exe, PID: " << notepad.id() 
                   << L", Handle: " << notepad.handle() << std::endl;
        
        auto info = notepad.get_info();
        if (info) { // info 是一个 std::optional
            std::wcout << L"  路径: " << info->exe_path << std::endl;
            std::wcout << L"  父进程ID: " << info->parent_pid << std::endl;
            // ...
        }

        Sleep(3000);
        // notepad.close(); // 可以主动关闭
    } // notepad 对象离开作用域时，其析构函数会自动调用 CloseHandle，无需手动管理
    
    std::wcout << L"notepad.exe 已关闭或其句柄已被自动释放。" << std::endl;
    
    return 0;
}
```

## 📜 API 参考

**重要**: 当函数返回 `false`, `0`, 或 `NULL` 表示失败时，可以立即调用 Windows API `GetLastError()` 来获取详细的错误代码。

| 函数名 | 描述 |
| :--- | :--- |
| `ProcUtils_ProcessExists(name_or_pid)` | 按进程名或 PID 查找进程。存在则返回第一个匹配的 PID，否则返回 0。 |
| `ProcUtils_ProcessClose(name_or_pid, exit_code)` | 强制终止一个进程。成功返回 `true`。 |
| `ProcUtils_ProcessWait(name, timeout_ms)` | 等待指定进程出现。出现则返回其 PID，超时返回 0。 |
| `ProcUtils_ProcessWaitClose(name_or_pid, timeout_ms)` | 等待指定进程结束。正常结束返回 `true`。 |
| `ProcUtils_ProcessGetPath(pid, out_path, path_size)` | 获取指定 PID 进程的完整路径。成功返回 `true`。 |
| `ProcUtils_CreateProcess(cmd, ...)` | **【新】** 创建一个新进程，并返回其 PID 和句柄。调用者需手动关闭句柄。 |
| `ProcUtils_LaunchProcess(cmd, ...)` | **【新】** 以“发后不理”模式启动进程，只返回 PID，无需管理句柄。|
| `ProcUtils_ProcessGetParent(name_or_pid)` | 获取指定进程的父进程 ID。成功返回父进程 PID，失败返回 0。 |
| `ProcUtils_ProcessSetPriority(name_or_pid, priority)` | 设置进程优先级 ('L', 'B', 'N', 'A', 'H', 'R')。成功返回 `true`。 |
| `ProcUtils_ProcessCloseTree(name_or_pid)` | 终止一个进程及其所有子进程。成功返回 `true`。 |
| `ProcUtils_FindAllProcesses(name, out_pids, size)` | 查找所有同名进程。返回找到的数量，出错返回 -1。|
| `ProcUtils_ProcessGetInfo(pid, out_info)` | 获取指定进程的详细信息。成功返回 `true`。|

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。
