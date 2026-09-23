cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER gcc)
endif()

project(RISCV-Emulator LANGUAGES C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_compile_options(-Wall -Wextra -Werror -pedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Test")
    add_compile_options(-g -O0)
    if (NOT WIN32)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Profile")
    add_compile_options(-O2 -g -fno-omit-frame-pointer)
endif()

file(GLOB LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

add_library(riscv_core STATIC ${LIB_SOURCES})
target_include_directories(riscv_core PUBLIC include)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Test")
    add_executable(riscv-emulator src/main.c)
    target_link_libraries(riscv-emulator PRIVATE riscv_core)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Test")
    enable_testing()

    macro(add_test_exe TEST_NAME)
        add_executable(${TEST_NAME}
            test/${TEST_NAME}.c
            test/unity/unity.c
        )
        target_include_directories(${TEST_NAME} PRIVATE include test/unity)
        target_link_libraries(${TEST_NAME} PRIVATE riscv_core)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endmacro()

    add_test_exe(test_memory)

    add_test_exe(test_binary_to_instruction)
    add_test_exe(test_loader)
    add_test_exe(test_api_emulator)
    add_test_exe(test_emulator)

    add_test_exe(test_api_debugger)
    add_test_exe(test_api_disassembler)
    add_test_exe(test_debugger_cli)
endif()