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
# find_package(clickhouse-cpp CONFIG REQUIRED)  # TODO: Fix clickhouse-cpp
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
# find_package(onnxruntime CONFIG REQUIRED)  # TODO: Add ONNX support later
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

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
