# 设置所需的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称和所用语言 (C++)
project(proc_utils CXX)

# --- 核心：灵活构建动态/静态库 ---
# 默认构建动态库，除非外部设置了 BUILD_SHARED_LIBS=OFF
option(BUILD_SHARED_LIBS "Build shared libraries (.dll)" ON)

# --- 添加所有源文件 ---
add_library(proc_utils
    src/proc_utils_core.cpp
    src/proc_utils_impl.cpp
    src/proc_utils_info.cpp
)

# 当构建动态库时，定义 PROC_UTILS_EXPORTS 宏
# 当构建静态库时，定义 PROC_UTILS_STATIC_LIB 宏
if(BUILD_SHARED_LIBS)
    target_compile_definitions(proc_utils PUBLIC
        $<$<BOOL:${BUILD_SHARED_LIBS}>:PROC_UTILS_EXPORTS>
    )
else()
    target_compile_definitions(proc_utils PUBLIC
        $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:PROC_UTILS_STATIC_LIB>
    )
endif()

# --- C++ 标准和公共属性 ---
set_target_properties(proc_utils PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# --- 头文件管理 ---
target_include_directories(proc_utils PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_include_directories(proc_utils PRIVATE
    src # 内部头文件
)

# --- 链接 Windows 库 ---
target_link_libraries(proc_utils PRIVATE psapi.lib)

# --- 为所有目标设置正确的 MSVC 运行时库 ---
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- 安装规则 (良好实践) ---
install(TARGETS proc_utils
    EXPORT proc_utils-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES include/proc_utils.h DESTINATION include)
install(EXPORT proc_utils-targets
    FILE proc_utils-targets.cmake
    NAMESPACE ProcUtils::
    DESTINATION lib/cmake/proc_utils
)