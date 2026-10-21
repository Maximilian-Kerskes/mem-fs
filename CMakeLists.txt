cmake_minimum_required(VERSION 3.20)

project(mem-fs
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# use this if youre targetting POSIX and not just c std
# target_compile_definitions(shell PRIVATE
#     _POSIX_C_SOURCE=200809L
# )

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

# Testing

enable_testing()

add_executable(${PROJECT_NAME}_test
    tests/test.c)

add_test(
    NAME ${PROJECT_NAME}_test
    COMMAND ${PROJECT_NAME}_test
)
