# Proc-Utils-FFI: LuaJIT FFI Windows 进程管理工具库

`proc_utils-ffi` 是一个基于 LuaJIT FFI 的、轻量级的 Windows 平台进程管理工具库。它是 C++ 库 `proc-utils` 的纯 Lua 重构版本，**无需编译**，仅依赖一个 `proc_utils_ffi.lua` 文件即可运行。

该库提供了一套简洁、健壮且符合 Lua 语言习惯的**面向对象接口**，用于常见的进程管理任务，如查找、创建、终止、等待以及获取详细信息。

## ✨ 功能特性

-   **现代化的 OOP 接口**:
    -   提供一个 `Process` 对象，通过 **RAII** 模式 (`__gc`元方法) **自动管理进程句柄**，杜绝资源泄漏。
    -   链式调用和直观的方法，如 `proc.exec("..."):wait_for_exit()`。
-   **零依赖、免编译**: 纯 `Lua` + `FFI` 实现，只需 `LuaJIT` 环境即可在 Windows 上运行。
-   **功能完整**: 完整实现了原始 C++ `proc-utils` 库的所有核心功能。
-   **友好的错误处理**: 失败时返回 `nil`、**错误码**和**人类可读的错误信息字符串**。
-   **API常量**: 提供 `proc.constants` 表，包含常用的 `SW_*`, `PROCESS_*` 等常量，告别魔法数字。
-   **进程查找与枚举**:
    -   通过工厂方法 `proc.open_by_pid()` 或 `proc.open_by_name()` 获取 `Process` 对象。
    -   静态方法 `proc.exists()` 和 `proc.find_all()` 用于快速查询。
-   **进程创建与执行**:
    -   `proc.exec()`: 创建新进程并返回一个自动管理句柄的 `Process` 对象。
    -   `proc.C_API.LaunchProcess`: 提供传统的“发后不理”模式。
    -   `proc.C_API.CreateProcessAsSystem`: 在当前活动桌面以 `SYSTEM` 权限创建进程。
-   **进程终止**:
    -   `process:terminate()`: 终止进程实例。
    -   `process:terminate_tree()`: 终止进程实例及其所有子进程。
-   **进程等待**:
    -   `process:wait_for_exit()`: 通过进程句柄高效等待进程结束。
    -   `proc.C_API.ProcessWait` 和 `proc.C_API.ProcessWaitClose` 提供基于名称的等待。
-   **信息获取**:
    -   `process:get_info()`: 以易于使用的 Lua table 形式返回进程的详细信息（路径、**完整命令行**、内存使用等）。
    -   `process:get_path()` 和 `process:get_command_line()` 提供快捷方法。
-   **低级API可用**: 为了完全的灵活性和兼容性，所有原始的C风格API都保留在 `proc.C_API` 命名空间下。

## 🚀 如何使用 (推荐的 OOP 风格)

将 `proc_utils_ffi.lua` 文件放入你的项目，并 `require` 它。

```lua
local proc = require("proc_utils_ffi")

if not proc._OS_SUPPORT then
  print("This library only runs on Windows.")
  return
end
```

### 示例 1: 创建、交互并自动资源管理

```lua
-- 使用常量提高可读性
local notepad, err_code, err_msg = proc.exec("notepad.exe", nil, proc.constants.SW_SHOW)

if notepad then
    print("Notepad 启动成功, PID: " .. notepad.pid)

    -- 获取详细信息
    local info, info_err_code, info_err_msg = notepad:get_info()
    if info then
        print("  路径: " .. info.exe_path)
        print("  命令行: " .. info.command_line)
    else
        print("获取信息失败: " .. info_err_msg)
    end

    print("等待 3 秒或直到进程关闭...")
    local exited = notepad:wait_for_exit(3000)
    
    if not exited then
        print("进程仍在运行，现在终止它。")
        notepad:terminate()
    else
        print("进程已自行关闭。")
    end
else
    print(string.format("启动 Notepad 失败, 错误: %s (代码: %d)", err_msg, err_code))
end
-- 此处 notepad 对象离开作用域，它的 __gc 元方法会自动被调用，
-- 确保 CloseHandle 被执行，无需手动管理句柄！
```

### 示例 2: 查找所有 Chrome 进程并打印信息

```lua
local chrome_pids = proc.find_all("chrome.exe")
if chrome_pids and #chrome_pids > 0 then
    print("找到 " .. #chrome_pids .. " 个 Chrome 进程:")
    for _, pid in ipairs(chrome_pids) do
        local p = proc.open_by_pid(pid)
        if p then
            local info = p:get_info()
            if info then
                print(string.format("  - PID: %d, Memory: %.2f MB", 
                    info.pid, (info.memory_usage_bytes or 0) / 1024 / 1024))
            end
        end -- p 对象在这里离开作用域并自动关闭句柄
    end
else
    print("没有找到 Chrome 进程。")
end
```

## 📜 API 参考 (高级 OOP 接口)

| 方法/函数 | 描述 |
| :--- | :--- |
| **工厂函数** | |
| `proc.exec(cmd, ...)` | 创建新进程，成功则返回 `Process` 对象，失败则返回 `nil, error_code, error_message`。 |
| `proc.open_by_pid(pid, access)` | 按 PID 打开进程，返回 `Process` 对象。 |
| `proc.open_by_name(name, access)`| 按名称打开进程，返回 `Process` 对象。 |
| `proc.current()`| 获取当前进程的 `Process` 对象。 |
| **静态工具函数** | |
| `proc.exists(name_or_pid)` | 检查进程是否存在，返回 PID 或 0。 |
| `proc.find_all(name)` | 查找所有同名进程，返回一个 PID 的 table。 |
| `proc.terminate_by_pid(pid, code)`| 静态方法：按 PID 终止进程。 |
| **Process 对象方法** | |
| `process.pid` | (属性) 进程ID。|
| `process:handle()` | (方法) 获取底层的 `HANDLE` (不建议手动关闭)。|
| `process:is_valid()`| 检查进程句柄是否有效。|
| `process:terminate(code)` | 终止此进程实例。 |
| `process:terminate_tree()`| 终止此进程实例及其所有子进程。|
| `process:wait_for_exit(timeout)`| 等待此进程结束。|
| `process:get_info()`| 返回一个包含详细信息的 Lua table。|
| `process:get_path()`| 返回进程的可执行文件路径字符串。|
| `process:get_command_line()`| 返回进程的完整命令行字符串。|
| **常量与低级接口** | |
| `proc.constants`| 一个包含常用 WinAPI 常量的 table。|
| `proc.C_API.*` | 包含所有原始的C风格API函数。 |


## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。