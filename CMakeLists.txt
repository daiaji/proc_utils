# 设置所需的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称和所用语言 (C++)
project(proc_utils CXX)

# 强制构建静态库 (STATIC)
add_library(proc_utils STATIC
    src/proc_utils_core.cpp
    src/proc_utils_impl.cpp
)

# 核心修正：当构建静态库时，必须定义 PROC_UTILS_STATIC_LIB 宏
# 这样 proc_utils.h 才会正确地将 API 声明为空，而不是 __declspec(dllimport)
target_compile_definitions(proc_utils PUBLIC PROC_UTILS_STATIC_LIB)

# 核心修正：强制所有配置都使用静态 C 运行时库 (/MT 或 /MTd)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# C++ 标准等公共属性
set_target_properties(proc_utils PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_sources(proc_utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/proc_utils.h>
    PRIVATE
        src/proc_utils_internal.h
)

target_include_directories(proc_utils PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

# 安装规则 (可选，但良好实践)
install(TARGETS proc_utils
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install(FILES include/proc_utils.h DESTINATION include)