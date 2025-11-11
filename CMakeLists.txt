# 设置所需的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称和所用语言 (C++)
project(proc_utils CXX)

#--------------------------------------------------------------------
# 启用 MSVC 静态代码分析
# 这会为生成的 Visual Studio 项目开启代码分析功能，并使用推荐的规则集。
if(MSVC)
    # 为所有目标启用代码分析
    set(CMAKE_VS_GLOBALS "EnableMicrosoftCodeAnalysis=true")
    
    # 使用微软推荐的原生规则集，这是一个很好的起点
    set(CMAKE_VS_GLOBALS "${CMAKE_VS_GLOBALS};CodeAnalysisRuleSet=NativeRecommendedRules.ruleset")
endif()
#--------------------------------------------------------------------

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

#--------------------------------------------------------------------
# 添加安装规则
# 这使得用户可以通过 "cmake --install" 命令将库文件和头文件导出到指定目录
install(TARGETS proc_utils
    RUNTIME DESTINATION bin     # .dll 文件
    LIBRARY DESTINATION lib     # .lib 文件 (Windows import library)
    ARCHIVE DESTINATION lib)    # .lib 文件 (static library, for completeness)

install(FILES include/proc_utils.h DESTINATION include) # 头文件
#--------------------------------------------------------------------