# Proc-Utils-FFI: LuaJIT FFI Windows 进程管理工具库

`proc_utils-ffi` 是一个基于 LuaJIT FFI 的、轻量级的 Windows 平台进程管理工具库。它是 C++ 库 `proc-utils` 的纯 Lua 重构版本。

该库利用 `lua-ffi-bindings` 提供的 Windows API 定义，提供了一套简洁、健壮且符合 Lua 语言习惯的**纯面向对象接口**，用于常见的进程管理任务。

## ✨ 功能特性

-   **现代化的 OOP 接口**:
    -   提供一个 `Process` 对象，通过 **RAII** 模式 (`__gc`元方法) **自动管理进程句柄**，杜绝资源泄漏。
    -   链式调用和直观的方法，如 `proc.exec("..."):wait_for_exit()`。
-   **免编译**: 纯 Lua 实现，**无需编译 C/C++ 代码**，只需 LuaJIT 环境即可在 Windows 上运行。
-   **功能完整**: 完整实现了原始 C++ `proc-utils` 库的所有核心功能。
-   **模块化设计**: 底层 WinAPI 定义通过 `lua-ffi-bindings` 模块加载，结构更清晰，易于维护。
-   **友好的错误处理**: 失败时返回 `nil`、**错误码**和**人类可读的错误信息字符串**。
-   **进程高级控制**:
    -   **SYSTEM 提权**: `proc.exec_as_system()` 支持在当前活动桌面以 SYSTEM 权限创建进程。
    -   **进程树终止**: `process:terminate_tree()` 可终止进程及其所有子进程。
    -   **命令行获取**: 支持读取任意进程的完整命令行参数（即使是 64 位进程）。

## 📦 安装与依赖

本项目依赖 `lua-ffi-bindings` 库来获取 Windows API 定义。

### 1. 添加子模块

在你的项目根目录下执行：

```bash
# 将 proc_utils_ffi.lua 下载到项目根目录或 lib 目录
# ...

# 添加依赖库到 vendor 目录
git submodule add https://github.com/daiaji/lua-ffi-bindings.git vendor/lua-ffi-bindings
git submodule update --init --recursive
```

### 2. 目录结构要求

`proc_utils_ffi.lua` 默认假设 `lua-ffi-bindings` 位于 `vendor/` 目录下。你的项目结构应类似如下：

```text
MyProject/
├── main.lua
├── proc_utils_ffi.lua
└── vendor/
    └── lua-ffi-bindings/
        └── Windows/
            └── ...
```

> **注意**: 如果你的目录结构不同，请修改 `proc_utils_ffi.lua` 顶部的 `req` 函数以匹配你的路径：
> ```lua
> local function req(name)
>     return require('your.custom.path.lua-ffi-bindings.Windows.sdk.' .. name)
> end
> ```

## 🚀 如何使用

在确保 `package.path` 能正确找到文件后，直接 `require`：

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

## 📜 API 参考

| 方法/函数 | 描述 |
| :--- | :--- |
| **工厂函数** | |
| `proc.exec(cmd, ...)` | 创建新进程，成功则返回 `Process` 对象，失败则返回 `nil, error_code, error_message`。 |
| `proc.exec_as_system(cmd, ...)` | 以 SYSTEM 权限创建进程，返回 `Process` 对象。 |
| `proc.open_by_pid(pid, access)` | 按 PID 打开进程，返回 `Process` 对象。 |
| `proc.open_by_name(name, access)`| 按名称打开进程，返回 `Process` 对象。 |
| `proc.current()`| 获取当前进程的 `Process` 对象。 |
| **静态工具函数** | |
| `proc.exists(name_or_pid)` | 检查进程是否存在，返回 PID 或 0。 |
| `proc.find_all(name)` | 查找所有同名进程，返回一个 PID 的 table。 |
| `proc.terminate_by_pid(pid, code)`| 静态方法：按 PID 终止进程。 |
| `proc.wait(name, timeout)` | 等待指定名称的进程**出现**。成功返回PID，超时返回 `nil`。 |
| `proc.wait_close(name_or_pid, timeout)` | 等待指定进程**关闭**。 |
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
| `process:set_priority(prio)` | 设置进程优先级。`prio` 是单字符: 'L' (低), 'B' (低于正常), 'N' (正常), 'A' (高于正常), 'H' (高), 'R' (实时)。 |
| **常量** | |
| `proc.constants`| 一个包含常用 WinAPI 常量的 table。|

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。