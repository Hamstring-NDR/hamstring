# Quick Start Build Guide

## Option 1: Build Without Full Dependencies (Demo Mode)

If you want to quickly test the code without installing all dependencies via vcpkg:

```bash
cd cpp

# Create a minimal build (no external dependencies)
cmake -B build-minimal \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=OFF

# This will fail - need to make dependencies optional first
```

## Option 2: Full Build with vcpkg (Recommended)

### Step 1: Install vcpkg

```bash
# Clone vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh
```

### Step 2: Configure CMake with vcpkg

```bash
cd /path/to/hamstring/cpp

# Configure with vcpkg toolchain
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Debug

# vcpkg will automatically download and build:
# - yaml-cpp
# - rdkafka  
# - clickhouse-cpp
# - boost
# - spdlog
# - nlohmann-json
# - openssl
# - gtest
# This may take 10-30 minutes on first run
```

### Step 3: Build

```bash
cmake --build build -j$(nproc)
```

### Step 4: Run

```bash
# Run demo
./build/examples/demo ../config.yaml

# Run logserver (when Kafka/ClickHouse are ready)
./build/src/logserver/logserver ../config.yaml

# Run tests
cd build && ctest
```

## Option 3: Docker Build (Easiest)

Create a simple Dockerfile:

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libssl-dev pkg-config

# Install vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh

WORKDIR /app
COPY cpp /app

# Build
RUN cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake && \
    cmake --build build -j$(nproc)

CMD ["./build/src/logserver/logserver", "config.yaml"]
```

Then:
```bash
docker build -t hamstring-cpp .
docker run hamstring-cpp
```

## What's the Error?

The error "Could not find a package configuration file provided by yaml-cpp" means CMake can't find the required libraries. You have 3 options:

1. **Use vcpkg** (recommended) - it will install everything automatically
2. **Install system packages** manually - but this is complex for all deps
3. **Make dependencies optional** - I can modify CMake to make some deps optional for demo builds

## Quick Fix: System Packages (macOS)

If you want to try with system packages:

```bash
brew install yaml-cpp librdkafka boost spdlog nlohmann-json openssl

# Then configure without vcpkg
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

**Note**: This may not work for all dependencies (clickhouse-cpp, onnxruntime not in brew)

## Recommended Next Step

I recommend using vcpkg as it's the most reliable method and matches the documentation. Would you like me to:

1. **Modify CMake to make dependencies optional** (for quick demo builds)
2. **Create a Docker setup** for easy building
3. **Help you install vcpkg** and walk through the full build
4. **Create a minimal example** that builds with no dependencies

Let me know which approach you prefer!
