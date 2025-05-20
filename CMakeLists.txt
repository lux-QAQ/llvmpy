cmake_minimum_required(VERSION 3.14)
project(llvmpy VERSION 0.1.0 LANGUAGES C CXX)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(FetchContent)

# --- 配置 LLVM SDK 的下载 ---
set(LLVMPY_LLVM_SDK_VERSION "v0.0.1") # 替换为您的SDK版本
set(LLVMPY_LLVM_SDK_PLATFORM "linux-x86_64") # 根据需要调整平台
set(LLVMPY_LLVM_SDK_REPO "lux-QAQ/llvmpy-llvm-sdk") # 替换为您的GitHub仓库
set(LLVMPY_LLVM_SDK_URL "https://github.com/${LLVMPY_LLVM_SDK_REPO}/releases/download/${LLVMPY_LLVM_SDK_VERSION}/llvmpy-llvm-sdk-${LLVMPY_LLVM_SDK_VERSION}-${LLVMPY_LLVM_SDK_PLATFORM}.tar.gz")
set(LLVMPY_LLVM_SDK_SHA256 "422815ccfa91e497ea0385816344fd55edd81b23a8d946a50831840ef842cfa5") # 非常重要：替换为实际的SHA256哈希值

# 声明 FetchContent 条目
FetchContent_Declare(
    llvm_sdk
    URL ${LLVMPY_LLVM_SDK_URL}
    URL_HASH SHA256=${LLVMPY_LLVM_SDK_SHA256}
    # SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/llvm_sdk-src" # FetchContent 会自动处理下载和解压
    # BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/llvm_sdk-build" # 对于预编译的SDK，我们不需要构建步骤
    # CONFIGURE_COMMAND "" # 不需要配置
    # BUILD_COMMAND ""     # 不需要构建
    # INSTALL_COMMAND ""   # 不需要安装
)



# 检查是否需要获取 LLVM SDK
# 您可以添加一个选项来允许用户提供本地已有的SDK路径，以避免每次都下载
option(LLVMPY_USE_LOCAL_LLVM_SDK "Use a local LLVM SDK instead of downloading" OFF)
set(LLVMPY_LOCAL_LLVM_SDK_PATH "" CACHE PATH "Path to the root of a local LLVM SDK")

if(LLVMPY_USE_LOCAL_LLVM_SDK AND EXISTS "${LLVMPY_LOCAL_LLVM_SDK_PATH}/lib/cmake/llvm/LLVMConfig.cmake")
    message(STATUS "Using local LLVM SDK from: ${LLVMPY_LOCAL_LLVM_SDK_PATH}")
    set(LLVM_DIR "${LLVMPY_LOCAL_LLVM_SDK_PATH}/lib/cmake/llvm")
else()
    message(STATUS "Attempting to fetch LLVM SDK from ${LLVMPY_LLVM_SDK_URL}")
    FetchContent_MakeAvailable(llvm_sdk)
    # FetchContent 会将内容解压到 CMAKE_BINARY_DIR/_deps/llvm_sdk-src (或类似路径)
    # 假设您的SDK包解压后的顶层目录是 llvmpy-llvm-sdk
    set(LLVM_SDK_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/llvm_sdk-src") # 确保这个路径与您tar包内的结构一致
    #find_package(LLVM REQUIRED CONFIG NO_CMAKE_SYSTEM_PATH) # <--- 添加 NO_CMAKE_SYSTEM_PATH
    set(LLVM_DIR "${LLVM_SDK_DOWNLOAD_DIR}/lib/cmake/llvm")
    message(STATUS "LLVM SDK fetched to: ${LLVM_SDK_DOWNLOAD_DIR}")
    message(STATUS "LLVM_DIR is now set to: ${LLVM_DIR}")
endif()


# 是否启用开发者LTO构建模式
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
    # --- 非LUXDEV模式：查找依赖库  ---
    find_package(PkgConfig REQUIRED)
    # 查找 libffi 
    pkg_check_modules(LIBFFI REQUIRED libffi>=3.0) # 检查 libffi 是否存在且版本足够
endif()

# 设置 RPATH 策略 
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:/usr/lib/x86_64-linux-gnu")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# 明确设置查找库的路径优先级 
list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH "/home/ljs/miniconda3")
list(INSERT CMAKE_SYSTEM_LIBRARY_PATH 0 "/usr/lib/x86_64-linux-gnu")

# 查找其他依赖库 
set(ZLIB_ROOT "/usr")
set(CMAKE_PREFIX_PATH "/usr" ${CMAKE_PREFIX_PATH})
find_package(ZLIB REQUIRED)
find_package(zstd QUIET)



#set(LLVM_DIR "/home/ljs/llvmall/lib/cmake/llvm")

find_package(LLVM
    REQUIRED
    CONFIG
    PATHS "${LLVM_DIR}"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 添加自己的include目录 
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 添加CodeGen源文件 
file(GLOB CODEGEN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CodeGen/*.cpp"
)

# 添加RunTime源文件 
file(GLOB RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RunTime/*.cpp"
)

# 添加可执行文件目标 
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

# 运行时支持库 
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

# 链接LLVM库到主程序 
llvm_map_components_to_libnames(llvm_libs support core irreader analysis instcombine
    scalaropts passes ipo transformutils)


# 链接主程序依赖项 
target_link_libraries(llvmpy
    PRIVATE
    ${llvm_libs}
    ZLIB::ZLIB

    llvmpy_runtime # 链接运行时库
)

# 如果找到zstd，也链接它 
if(zstd_FOUND)
    target_link_libraries(llvmpy PRIVATE zstd::libzstd_shared)
endif()

# 设置C/C++标准 
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
