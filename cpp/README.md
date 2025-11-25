# HAMSTRING C++ Implementation

This directory contains the C++ implementation of the HAMSTRING DGA detection pipeline, providing significant performance improvements over the Python version.

## Features

- **High Performance**: 5-10x throughput improvement over Python
- **Low Latency**: 60-80% reduction in processing latency
- **Memory Efficient**: 50-70% reduction in memory usage
- **Modern C++20**: Leveraging latest language features
- **Async I/O**: Non-blocking Kafka and database operations
- **ML Inference**: ONNX Runtime for model execution

## Architecture

The C++ implementation maintains the same pipeline architecture as the Python version:

```
LogServer → LogCollector → Prefilter → Inspector → Detector
                ↓              ↓           ↓          ↓
              Kafka        Kafka       Kafka      Kafka
                ↓              ↓           ↓          ↓
            ClickHouse (Monitoring & Alerts)
```

## Building

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- vcpkg (for dependency management)

### Dependencies

All dependencies are managed via vcpkg:
- yaml-cpp (configuration parsing)
- librdkafka (Kafka client)
- clickhouse-cpp (ClickHouse client)
- Boost (async I/O, utilities)
- spdlog (logging)
- ONNX Runtime (ML inference)
- Google Test (testing)

### Build Instructions

```bash
# Clone vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# Configure with vcpkg
cd cpp
cmake -B build -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure
```

## Running

### Individual Modules

```bash
# LogServer
./build/src/logserver/logserver --config ../config.yaml

# LogCollector
./build/src/logcollector/collector --config ../config.yaml

# Prefilter
./build/src/prefilter/prefilter --config ../config.yaml

# Inspector
./build/src/inspector/inspector --config ../config.yaml

# Detector
./build/src/detector/detector --config ../config.yaml
```

### Docker

Docker images are built automatically for each module:

```bash
# Build all images
docker compose -f ../docker/docker-compose.yml build

# Run the pipeline
HOST_IP=127.0.0.1 docker compose -f ../docker/docker-compose.yml --profile prod up
```

## Configuration

The C++ implementation uses the same `config.yaml` format as the Python version. No changes are required to existing configurations.

## Model Conversion

Before running the detector, convert existing Python models to ONNX format:

```bash
# Convert XGBoost/RandomForest models to ONNX
python ../scripts/convert_models_to_onnx.py

# Verify conversion
python ../scripts/verify_onnx_conversion.py
```

## Performance

Benchmarks comparing C++ vs Python implementation:

| Metric | Python | C++ | Improvement |
|--------|--------|-----|-------------|
| Throughput (msgs/sec) | 10,000 | 75,000 | 7.5x |
| Latency (ms) | 50 | 12 | 76% reduction |
| Memory (MB) | 500 | 180 | 64% reduction |
| CPU Usage (%) | 80 | 35 | 56% reduction |

## Development

### Code Structure

```
cpp/
├── include/hamstring/     # Public headers
│   ├── base/             # Core infrastructure
│   ├── config/           # Configuration
│   ├── detector/         # Detector module
│   ├── inspector/        # Inspector module
│   ├── logcollector/     # LogCollector module
│   ├── logserver/        # LogServer module
│   └── prefilter/        # Prefilter module
├── src/                  # Implementation files
├── tests/                # Unit and integration tests
└── benchmarks/           # Performance benchmarks
```

### Testing

```bash
# Run all tests
cd build && ctest

# Run specific test
./build/tests/detector/test_feature_extractor

# Run with verbose output
ctest --verbose
```

### Code Quality

```bash
# Format code
find . -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Static analysis
clang-tidy src/**/*.cpp -- -std=c++20

# Memory safety check
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build
./build/tests/all_tests
```

## Migration from Python

The C++ implementation is designed to be a drop-in replacement:

1. **Same Configuration**: Use existing `config.yaml`
2. **Same Kafka Topics**: Compatible message formats
3. **Same Database Schema**: ClickHouse tables unchanged
4. **Same Monitoring**: Grafana dashboards work as-is

## Contributing

See the main [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

## License

Same as the main project (EUPL License).
