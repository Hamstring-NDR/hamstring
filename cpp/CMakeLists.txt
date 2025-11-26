cmake_minimum_required(VERSION 3.20)
project(hamstring VERSION 1.0.0 LANGUAGES CXX)

# Enable vcpkg manifest mode
set(VCPKG_MANIFEST_MODE ON)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)  # Removed -Werror for now
endif()

# Build types
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Find packages
find_package(yaml-cpp CONFIG REQUIRED)
find_package(RdKafka CONFIG REQUIRED)
# find_package(clickhouse-cpp CONFIG REQUIRED)  # No CMake config provided by vcpkg
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
# find_package(onnxruntime CONFIG REQUIRED)  # Optional for Detector module
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# Manually find ClickHouse C++ library (no CMake config provided by vcpkg)
find_library(CLICKHOUSE_CPP_LIB
    NAMES clickhouse-cpp-lib libclickhouse-cpp-lib
    PATHS ${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
    NO_DEFAULT_PATH
)
if(NOT CLICKHOUSE_CPP_LIB)
    message(FATAL_ERROR "ClickHouse C++ library not found")
endif()
message(STATUS "Found ClickHouse C++ library: ${CLICKHOUSE_CPP_LIB}")

# ClickHouse include directory
set(CLICKHOUSE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include)
message(STATUS "ClickHouse include directory: ${CLICKHOUSE_INCLUDE_DIR}")

# Find ClickHouse dependencies (zstd and cityhash)
find_library(ZSTD_LIB
    NAMES zstd libzstd
    PATHS ${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
    NO_DEFAULT_PATH
)
find_library(CITYHASH_LIB
    NAMES cityhash libcityhash
    PATHS ${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
    NO_DEFAULT_PATH
)
if(NOT ZSTD_LIB OR NOT CITYHASH_LIB)
    message(FATAL_ERROR "ClickHouse dependencies not found (zstd: ${ZSTD_LIB}, cityhash: ${CITYHASH_LIB})")
endif()
message(STATUS "Found zstd: ${ZSTD_LIB}")
message(STATUS "Found cityhash: ${CITYHASH_LIB}")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(src)
add_subdirectory(examples)

# Testing
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()

# Benchmarks
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Installation
install(DIRECTORY include/ DESTINATION include)
