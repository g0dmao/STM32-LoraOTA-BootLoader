cmake_minimum_required(VERSION 3.22)

# ============================================================
#  Host-side tools — 独立于 STM32 交叉编译固件
#
#  构建方法：
#    cd tools && mkdir -p build && cd build && cmake .. && make
#
#  可执行文件生成在 tools/bin/ 目录下。
# ============================================================

project(tools C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# ============================================================
#  libhydrogen — 静态库（主机编译，非交叉编译）
# ============================================================
add_library(hydrogen STATIC
    libhydrogen/hydrogen.c
)

target_include_directories(hydrogen PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/libhydrogen"
    "${CMAKE_CURRENT_SOURCE_DIR}/libhydrogen/impl"
)

# ============================================================
#  keygen — Ed25519 密钥对生成工具
# ============================================================
add_executable(keygen
    core/keygen.c
)

target_link_libraries(keygen PRIVATE hydrogen)

# ============================================================
#  binpkg — 固件签名打包工具
# ============================================================
add_executable(binpkg
    core/binpkg.c
)

target_include_directories(binpkg PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
)

target_link_libraries(binpkg PRIVATE hydrogen)
