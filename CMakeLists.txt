cmake_minimum_required(VERSION 3.20)
project(fastkv VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support (e.g., clangd, VS Code IntelliSense)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure output directories for clean build layouts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Setup Compiler Warning Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wunused
        -Wconversion
        -Wsign-conversion
    )
endif()

# Setup Sanitizers option
set(FASTKV_SANITIZE "none" CACHE STRING "Sanitizer type to use: none, address, thread")
set_property(CACHE FASTKV_SANITIZE PROPERTY STRINGS "none" "address" "thread")

if(NOT FASTKV_SANITIZE STREQUAL "none")
    if(FASTKV_SANITIZE STREQUAL "address")
        message(STATUS "FastKV: Enabling AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan)")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    elseif(FASTKV_SANITIZE STREQUAL "thread")
        message(STATUS "FastKV: Enabling ThreadSanitizer (TSan)")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    else()
        message(FATAL_ERROR "FastKV: Unsupported sanitizer type: ${FASTKV_SANITIZE}")
    endif()
endif()

# Declare and load dependencies: GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Prevent GTest from overriding parent compiler settings (especially on Windows)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Exclude from all to avoid installing gtest targets with the main project
FetchContent_MakeAvailable(googletest)

# Enable CTest framework for automated execution of target test suites
enable_testing()

# Include subdirectories
add_subdirectory(src)
add_subdirectory(tests)
