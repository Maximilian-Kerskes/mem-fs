cmake_minimum_required(VERSION 3.20)

project(mem-fs
    VERSION 0.1.0
    LANGUAGES C
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED IMPORTED_TARGET fuse3)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(${PROJECT_NAME}
    src/main.c
    src/fuse_ops.c
    src/fs.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# use this if youre targetting POSIX and not just c std
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _POSIX_C_SOURCE=200809L
    _GNU_SOURCE
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    PkgConfig::FUSE3
)

# Testing

enable_testing()

add_executable(${PROJECT_NAME}_test
    tests/test.c
    src/fuse_ops.c
    src/fs.c
)

add_test(
    NAME ${PROJECT_NAME}_test
    COMMAND ${PROJECT_NAME}_test
)

target_include_directories(${PROJECT_NAME}_test PRIVATE
    include
)

target_compile_definitions(${PROJECT_NAME}_test PRIVATE
    _POSIX_C_SOURCE=200809L
    _GNU_SOURCE
)

target_link_libraries(${PROJECT_NAME}_test PRIVATE
    PkgConfig::FUSE3
)
