# Proc-Utils: C++ Win32 进程管理工具库

`proc_utils` 是一个轻量级的、无外部依赖的动态链接库 (DLL)，为 Windows 平台提供了一套简洁、易用的 C 语言接口，用于常见的进程管理任务。

该库旨在简化与进程相关的操作，如查找、启动、终止、等待以及修改进程属性，使其可以被 C/C++、Python、C# 等多种语言轻松调用。

## ✨ 功能特性

-   **进程查找**: 按进程名或 PID 检查进程是否存在。
-   **进程执行**: 启动新进程，支持设置工作目录、窗口显示模式，并可选择同步等待。
-   **进程终止**: 强制终止指定进程，或终止一个完整的进程树（包括所有子进程）。
-   **进程等待**:
    -   同步等待，直到指定名称的进程出现。
    -   同步等待，直到指定进程结束。
-   **信息获取**:
    -   获取进程可执行文件的完整路径。
    -   获取指定进程的父进程 PID。
-   **属性修改**: 设置进程的CPU优先级（如：低、正常、高、实时）。
-   **纯 C 接口**: 导出的函数均为 `extern "C"`，确保了跨语言调用的兼容性。

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

`proc_utils.dll` 可以被任何支持标准 C 调用约定的语言加载和使用。

### 在 C/C++ 中使用

您需要 `proc_utils.dll`、`proc_utils.lib` 和 `proc_utils.h` 这三个文件。

1.  将 `proc_utils.h` 包含到您的源代码中。
2.  将 `proc_utils.lib` 链接到您的项目。
3.  确保 `proc_utils.dll` 在程序运行时可以被找到（例如，与您的可执行文件放在同一目录）。

**示例 (`main.cpp`)**:

```cpp
#include <iostream>
#include "proc_utils.h" // 引入头文件
#include <windows.h> // For SW_SHOW

// 链接到 proc_utils.lib
#pragma comment(lib, "proc_utils.lib")

int main() {
    const wchar_t* process_name = L"notepad.exe";

    std::wcout << L"正在启动 " << process_name << L"..." << std::endl;

    // 1. 启动记事本
    unsigned int pid = ProcUtils_Exec(process_name, nullptr, SW_SHOW, false, nullptr);

    if (pid > 0) {
        std::wcout << L"成功启动，PID: " << pid << std::endl;

        // 等待3秒
        Sleep(3000);

        // 2. 检查进程是否存在
        unsigned int found_pid = ProcUtils_ProcessExists(process_name);
        if (found_pid == pid) {
            std::wcout << L"进程检查成功，PID 匹配。" << std::endl;
        } else {
            std::wcerr << L"错误：进程检查失败！" << std::endl;
        }

        // 3. 关闭记事本
        std::wcout << L"正在关闭 " << process_name << L"..." << std::endl;
        if (ProcUtils_ProcessClose(process_name, 0)) {
            std::wcout << L"关闭成功。" << std::endl;
        } else {
            std::wcerr << L"错误：关闭失败！" << std::endl;
        }

    } else {
        std::wcerr << L"错误：启动 " << process_name << L" 失败！" << std::endl;
    }

    return 0;
}
```

### 在 Python 中使用

在 Python 中，可以使用内置的 `ctypes` 库来加载 DLL 并调用其函数。这是验证和使用该库的最快捷方式。

1.  将 `proc_utils.dll` 与您的 Python 脚本放在同一个目录下。

**示例 (`test_proc_utils.py`)**:

```python
import ctypes
import os
import time

# --- 配置 ---
DLL_FILENAME = "proc_utils.dll"
PROCESS_NAME = "notepad.exe" # 使用记事本作为稳定的测试目标

def setup_library_functions(lib):
    """为库中的所有函数定义参数类型和返回类型。"""
    lib.ProcUtils_ProcessExists.argtypes = [ctypes.c_wchar_p]
    lib.ProcUtils_ProcessExists.restype = ctypes.c_uint

    lib.ProcUtils_ProcessClose.argtypes = [ctypes.c_wchar_p, ctypes.c_uint]
    lib.ProcUtils_ProcessClose.restype = ctypes.c_bool

    lib.ProcUtils_ProcessWait.argtypes = [ctypes.c_wchar_p, ctypes.c_int]
    lib.ProcUtils_ProcessWait.restype = ctypes.c_uint

    lib.ProcUtils_ProcessWaitClose.argtypes = [ctypes.c_wchar_p, ctypes.c_int]
    lib.ProcUtils_ProcessWaitClose.restype = ctypes.c_bool

    lib.ProcUtils_ProcessGetPath.argtypes = [ctypes.c_uint, ctypes.c_wchar_p, ctypes.c_int]
    lib.ProcUtils_ProcessGetPath.restype = ctypes.c_bool

    lib.ProcUtils_Exec.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_int, ctypes.c_bool, ctypes.c_wchar_p]
    lib.ProcUtils_Exec.restype = ctypes.c_uint

    lib.ProcUtils_ProcessGetParent.argtypes = [ctypes.c_wchar_p]
    lib.ProcUtils_ProcessGetParent.restype = ctypes.c_uint

    lib.ProcUtils_ProcessSetPriority.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar]
    lib.ProcUtils_ProcessSetPriority.restype = ctypes.c_bool


def main():
    """主测试流程。"""
    dll_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), DLL_FILENAME)
    if not os.path.exists(dll_path):
        print(f"[错误] DLL 文件未找到: {dll_path}")
        return

    try:
        lib = ctypes.WinDLL(dll_path)
        setup_library_functions(lib)
        print("[成功] DLL 加载并初始化成功。")
    except (OSError, AttributeError) as e:
        print(f"[错误] 加载 DLL 或定义函数失败: {e}")
        return
        
    print(f"\n--- 开始使用 '{PROCESS_NAME}' 进行测试 ---")

    # 确保测试前环境干净
    if lib.ProcUtils_ProcessExists(PROCESS_NAME):
        print(f"检测到已存在的 '{PROCESS_NAME}'，正在关闭...")
        lib.ProcUtils_ProcessClose(PROCESS_NAME, 0)
        lib.ProcUtils_ProcessWaitClose(PROCESS_NAME, 3000)

    # 1. 启动进程
    pid = lib.ProcUtils_Exec(PROCESS_NAME, None, 5, False, None) # SW_SHOW = 5
    if not pid:
        print("[失败] 启动进程失败。")
        return
    print(f"[成功] 启动进程，PID: {pid}")
    time.sleep(1)

    # 2. 获取路径
    path_buffer = ctypes.create_unicode_buffer(260)
    if lib.ProcUtils_ProcessGetPath(pid, path_buffer, 260):
        print(f"[成功] 获取到路径: {path_buffer.value}")
    else:
        print("[失败] 获取路径失败。")

    # 3. 关闭进程
    if lib.ProcUtils_ProcessClose(str(pid), 0):
        print(f"[成功] 已发送关闭指令给 PID {pid}。")
    else:
        print("[失败] 关闭进程失败。")

    # 4. 等待进程关闭
    if lib.ProcUtils_ProcessWaitClose(PROCESS_NAME, 5000):
        print("[成功] 确认进程已关闭。")
    else:
        print("[失败] 等待进程关闭超时。")

    print("\n--- 测试完成 ---")


if __name__ == "__main__":
    main()
```

## 📜 API 参考

以下是所有导出的函数列表：

| 函数名 | 描述 |
| :--- | :--- |
| `ProcUtils_ProcessExists(name_or_pid)` | 按进程名或 PID 查找进程。存在则返回 PID，否则返回 0。 |
| `ProcUtils_ProcessClose(name_or_pid, exit_code)` | 强制终止一个进程。成功返回 `true`。 |
| `ProcUtils_ProcessWait(name, timeout_ms)` | 等待指定进程出现。出现则返回其 PID，超时返回 0。 |
| `ProcUtils_ProcessWaitClose(name_or_pid, timeout_ms)` | 等待指定进程结束。正常结束返回 `true`。 |
| `ProcUtils_ProcessGetPath(pid, out_path, path_size)` | 获取指定 PID 进程的完整路径。成功返回 `true`。 |
| `ProcUtils_Exec(cmd, work_dir, show, wait, desktop)` | 执行一个外部程序。成功启动则返回其 PID，失败返回 0。 |
| `ProcUtils_ProcessGetParent(name_or_pid)` | 获取指定进程的父进程 ID。成功返回父进程 PID，失败返回 0。 |
| `ProcUtils_ProcessSetPriority(name_or_pid, priority)` | 设置进程优先级 ('L', 'B', 'N', 'A', 'H', 'R')。成功返回 `true`。 |
| `ProcUtils_ProcessCloseTree(name_or_pid)` | 终止一个进程及其所有子进程。成功返回 `true`。 |

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。