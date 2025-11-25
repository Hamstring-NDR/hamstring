# Building HAMSTRING C++

## Quick Start

```bash
# 1. Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh

# 2. Build the project
cd /path/to/hamstring/cpp
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# 3. Run the demo
./build/examples/demo ../config.yaml

# 4. Run tests
cd build && ctest --output-on-failure
```

## Prerequisites

### Required Tools
- CMake 3.20 or higher
- C++20 compatible compiler:
  - GCC 10+ (Linux)
  - Clang 12+ (macOS/Linux)
  - MSVC 2019+ (Windows)
- vcpkg (package manager)

### System Dependencies (macOS)
```bash
brew install cmake pkg-config openssl
```

### System Dependencies (Ubuntu/Debian)
```bash
sudo apt-get install build-essential cmake pkg-config libssl-dev
```

## Detailed Build Steps

### 1. Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Add to PATH (optional)
export PATH="$PATH:$(pwd)"
```

### 2. Configure the Project

```bash
cd /path/to/hamstring/cpp

# Configure with vcpkg
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

# For debug builds
cmake -B build-debug \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Debug
```

### 3. Build

```bash
# Build all targets
cmake --build build -j$(nproc)

# Build specific target
cmake --build build --target demo -j$(nproc)
```

### 4. Run

```bash
# Run demo (from cpp directory)
./build/examples/demo ../config.yaml

# Run feature extraction test
./build/tests/detector/test_feature_extractor
```

## Build Options

### Enable/Disable Testing
```bash
cmake -B build -DBUILD_TESTING=OFF
```

### Enable Benchmarks
```bash
cmake -B build -DBUILD_BENCHMARKS=ON
cmake --build build --target bench_feature_extraction
```

### Compiler Flags

The project uses strict compilation settings:
- `-Wall -Wextra -Wpedantic` - All warnings
- `-Werror` - Treat warnings as errors
- `-O3` - Maximum optimization (Release)
- `-g -O0` - Debug symbols, no optimization (Debug)

## Troubleshooting

### Missing Dependencies

If vcpkg fails to install dependencies:
```bash
# Clear vcpkg cache
rm -rf ~/vcpkg/buildtrees ~/vcpkg/packages

# Retry installation
cd /path/to/hamstring/cpp
rm -rf build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### OpenSSL Not Found (macOS)

```bash
# Install OpenSSL via Homebrew
brew install openssl

# Configure with OpenSSL path
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl
```

### Compiler Version Issues

```bash
# Check compiler version
g++ --version  # Should be 10+
clang++ --version  # Should be 12+

# Use specific compiler
cmake -B build \
    -DCMAKE_CXX_COMPILER=g++-11 \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Testing

### Run All Tests
```bash
cd build
ctest --output-on-failure
```

### Run Specific Test
```bash
./tests/detector/test_feature_extractor
./tests/base/test_utils
```

### Verbose Test Output
```bash
ctest --verbose
```

## Installation

```bash
# Install to system (requires sudo)
cd build
sudo cmake --install .

# Install to custom prefix
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/hamstring
cmake --build build
cmake --install build
```

## Development

### Generate compile_commands.json (for IDE)
```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -s build/compile_commands.json .
```

### Code Formatting
```bash
find . -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

### Static Analysis
```bash
clang-tidy src/**/*.cpp -- -std=c++20
```

## Next Steps

After building successfully:

1. **Convert Models**: Convert Python models to ONNX
   ```bash
   python ../scripts/convert_models_to_onnx.py --verify
   ```

2. **Run Demo**: Test the example program
   ```bash
   ./build/examples/demo ../config.yaml
   ```

3. **Implement Modules**: Continue with pipeline module implementation
   - LogServer
   - LogCollector
   - Prefilter
   - Inspector
   - Detector
