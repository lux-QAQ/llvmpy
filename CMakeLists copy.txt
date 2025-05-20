cmake_minimum_required(VERSION 3.10.0)
project(llvmpy VERSION 0.1.0 LANGUAGES C CXX)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加一个选项来控制是否启用开发者LTO构建模式
option(LUXDEV "启用自定义LTO构建，使用硬编码的开发路径和O0 LTO标志" OFF)

if(LUXDEV)
    message(STATUS "LUXDEV模式已启用: 使用自定义LTO库和O0 LTO标志进行调试。")
    # --- LUXDEV模式：设置编译器和LTO优化标志 ---
    set(CMAKE_C_COMPILER "clang")
    set(CMAKE_CXX_COMPILER "clang++")
    set(CMAKE_AR "llvm-ar")
    set(CMAKE_RANLIB "llvm-ranlib")

    # LTO 和优化标志 (O0 用于调试)
    set(DEV_LTO_COMPILE_FLAGS "-O0 -flto=full")
    set(DEV_LTO_LINK_FLAGS "-fuse-ld=lld -flto=full -O0") # -O0 也应用于链接阶段的LTO

    # 将LTO标志附加到全局标志
    # 注意: 这会附加到任何由 CMAKE_BUILD_TYPE 设置的标志之后。
    # 对于调试LTO, -O0 通常会覆盖其他优化级别。
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${DEV_LTO_COMPILE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEV_LTO_COMPILE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${DEV_LTO_LINK_FLAGS}")

    # --- LUXDEV模式：指定自定义编译的 libffi 和 GMP ---
    set(CUSTOM_LIBFFI_INCLUDE_DIR "/opt/custom_lto_libs/libffi/include")
    set(CUSTOM_LIBFFI_LIBRARY "/opt/custom_lto_libs/libffi/lib/libffi.a")
    message(STATUS "LUXDEV: 使用自定义 libffi 头文件: ${CUSTOM_LIBFFI_INCLUDE_DIR}")
    message(STATUS "LUXDEV: 使用自定义 libffi 库: ${CUSTOM_LIBFFI_LIBRARY}")

    set(CUSTOM_GMP_INCLUDE_DIR "/opt/custom_lto_libs/gmp/include")
    set(CUSTOM_GMP_LIBRARY "/opt/custom_lto_libs/gmp/lib/libgmp.a")
    message(STATUS "LUXDEV: 使用自定义 GMP 头文件: ${CUSTOM_GMP_INCLUDE_DIR}")
    message(STATUS "LUXDEV: 使用自定义 GMP 库: ${CUSTOM_GMP_LIBRARY}")

else()
    message(STATUS "LUXDEV模式未启用: 使用标准库查找和默认编译标志。")
    # --- 非LUXDEV模式：查找依赖库 (原始行为) ---
    find_package(PkgConfig REQUIRED)
    # 查找 libffi (原始行为)
    pkg_check_modules(LIBFFI REQUIRED libffi>=3.0) # 检查 libffi 是否存在且版本足够
endif()

# 设置 RPATH 策略 (保持不变)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:/usr/lib/x86_64-linux-gnu")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# 明确设置查找库的路径优先级 (保持不变)
list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH "/home/ljs/miniconda3")
list(INSERT CMAKE_SYSTEM_LIBRARY_PATH 0 "/usr/lib/x86_64-linux-gnu")

# 查找其他依赖库 (保持不变)
set(ZLIB_ROOT "/usr")
set(CMAKE_PREFIX_PATH "/usr" ${CMAKE_PREFIX_PATH})
find_package(ZLIB REQUIRED)
find_package(zstd QUIET)

# 显式设置 LLVM_DIR 路径 (保持不变)
set(LLVM_DIR "/home/ljs/llvmall/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 添加自己的include目录 (保持不变)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 添加CodeGen源文件 (保持不变)
file(GLOB CODEGEN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CodeGen/*.cpp"
)

# 添加RunTime源文件 (保持不变)
file(GLOB RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RunTime/*.cpp"
)

# 添加可执行文件目标 (保持不变)
add_executable(llvmpy
    src/main.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Ast.cpp
    src/ObjectType.cpp
    src/TypeOperations.cpp
    src/ObjectLifecycle.cpp
    ${CODEGEN_SOURCES}
)

# 运行时支持库 (保持不变)
add_library(llvmpy_runtime STATIC
    src/entry.c
    ${RUNTIME_SOURCES}
)

# --- 条件化地为 llvmpy_runtime 添加头文件目录和链接库 ---
if(LUXDEV)
    target_include_directories(llvmpy_runtime PUBLIC
        ${CUSTOM_LIBFFI_INCLUDE_DIR}
        ${CUSTOM_GMP_INCLUDE_DIR}
    )
    target_link_libraries(llvmpy_runtime PUBLIC
        ${CUSTOM_LIBFFI_LIBRARY}
        ${CUSTOM_GMP_LIBRARY}
    )
else()
    # 原始行为：仅 libffi
    target_include_directories(llvmpy_runtime PUBLIC ${LIBFFI_INCLUDE_DIRS})
    target_link_libraries(llvmpy_runtime PUBLIC ${LIBFFI_LIBRARIES})
endif()

# 链接LLVM库到主程序 (保持不变)
llvm_map_components_to_libnames(llvm_libs support core irreader analysis instcombine
    scalaropts passes ipo transformutils)

# 链接主程序依赖项 (保持不变)
target_link_libraries(llvmpy
    PRIVATE
    ${llvm_libs}
    ZLIB::ZLIB
    llvmpy_runtime # 链接我们自己的运行时库
)

# 如果找到zstd，也链接它 (保持不变)
if(zstd_FOUND)
    target_link_libraries(llvmpy PRIVATE zstd::libzstd_shared)
endif()

# 设置C/C++标准 (保持不变)
set_target_properties(llvmpy PROPERTIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    C_STANDARD ${CMAKE_C_STANDARD}
    C_STANDARD_REQUIRED ON
)
set_target_properties(llvmpy_runtime PROPERTIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    C_STANDARD ${CMAKE_C_STANDARD}
    C_STANDARD_REQUIRED ON
)

# 可选：添加安装规则 (保持不变)
# install(TARGETS llvmpy DESTINATION bin)
# install(TARGETS llvmpy_runtime DESTINATION lib)
# install(DIRECTORY include/ DESTINATION include)

# 打印最终的编译和链接标志 (根据LUXDEV状态)
if(LUXDEV)
    message(STATUS "LUXDEV C_FLAGS: ${CMAKE_C_FLAGS}")
    message(STATUS "LUXDEV CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
    message(STATUS "LUXDEV EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
else()
    # 在非LUXDEV模式下，这些标志将由CMAKE_BUILD_TYPE等决定
    message(STATUS "Non-LUXDEV C_FLAGS_RELEASE: ${CMAKE_C_FLAGS_RELEASE}") # 示例，实际可能看Debug或未设置
    message(STATUS "Non-LUXDEV CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
    message(STATUS "Non-LUXDEV EXE_LINKER_FLAGS_RELEASE: ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
endif()
