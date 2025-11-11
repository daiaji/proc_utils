# 设置所需的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称和所用语言 (C++)
project(proc_utils CXX)

# 将模块编译成一个动态链接库 (DLL)
add_library(proc_utils SHARED
    src/proc_utils_core.cpp
    src/proc_utils_impl.cpp
)

#--------------------------------------------------------------------
# 现代 CMake 推荐的方式：为具体目标设置所需的 C++ 标准
# 这比全局设置更加清晰和健壮
set_target_properties(proc_utils PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
#--------------------------------------------------------------------

# 将头文件与目标关联
target_sources(proc_utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/proc_utils.h>
    PRIVATE
        src/proc_utils_internal.h
)

# 添加 include 目录
target_include_directories(proc_utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 定义导出宏
target_compile_definitions(proc_utils PRIVATE PROC_UTILS_EXPORTS)