cmake_minimum_required(VERSION 3.10.0)
project(llvmpy VERSION 0.1.0 LANGUAGES C CXX)

# 设置 RPATH 策略
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:/usr/lib/x86_64-linux-gnu")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# 静态链接标准库
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# 查找依赖库
find_package(ZLIB REQUIRED)
find_package(zstd QUIET)  # QUIET参数使其找不到时不会报错

# 查找LLVM包
find_package(LLVM REQUIRED CONFIG)

# 设置LLVM相关参数
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# 添加LLVM头文件
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 添加自己的include目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 添加源文件
add_executable(llvmpy 
    main.cpp
    runtime.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/ast.cpp
    src/ObjectType.cpp
    src/ObjectRuntime.cpp
)

# 运行时支持库 (编译为静态库)
add_library(llvmpy_runtime STATIC
    runtime.cpp
)

# 链接LLVM库
llvm_map_components_to_libnames(llvm_libs support core irreader analysis instcombine 
    scalaropts passes ipo transformutils)
target_link_libraries(llvmpy ${llvm_libs} ZLIB::ZLIB llvmpy_runtime)

# 如果找到zstd，也链接它
if(zstd_FOUND)
    target_link_libraries(llvmpy zstd::libzstd_shared)
endif()

# 设置C++标准
set_target_properties(llvmpy PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)