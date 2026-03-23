# HAMSTRING - NixOS Deployment Guide

## Overview

HAMSTRING uses Nix flakes for reproducible builds and cross-platform deployment instead of Docker.

## Quick Start

### Development Environment

```bash
# Enter development shell
nix develop

# Build all modules
cmake -B build -S cpp
cmake --build build -j
```

### Build Individual Modules

```bash
# Build specific module
nix build .#logserver
nix build .#logcollector
nix build .#prefilter
nix build .#inspector

# Build all modules
nix build

# Run directly
nix run .#logserver -- config.yaml
```

### Cross-Platform Build

```bash
# Build for Linux (from macOS)
nix build .#logserver --system x86_64-linux

# Build for macOS ARM
nix build .#logserver --system aarch64-darwin

# Build for all platforms
nix build .#logserver --system x86_64-linux --system aarch64-darwin
```

## NixOS Deployment

### Configuration

Add to your `configuration.nix`:

```nix
{
  inputs.hamstring.url = "github:yourusername/hamstring";
  
  # ... in your configuration:
  
  imports = [
    hamstring.nixosModules.default
  ];
  
  services.hamstring = {
    enable = true;
    configFile = ./config.yaml;
    
    kafkaBrokers = [ "localhost:19092" "localhost:19093" ];
    
    modules = {
      logserver.enable = true;
      logcollector.enable = true;
      prefilter.enable = true;
      inspector.enable = true;
    };
  };
  
  # Kafka service (example)
  services.apache-kafka = {
    enable = true;
    # ... kafka config
  };
}
```

### Deploy

```bash
# Rebuild NixOS configuration
sudo nixos-rebuild switch

# Check service status
systemctl status hamstring-logserver
systemctl status hamstring-logcollector
systemctl status hamstring-prefilter
systemctl status hamstring-inspector

# View logs
journalctl -u hamstring-inspector -f
```

## Development Workflow

### Enter Dev Shell

```bash
nix develop
```

Provides:
- C++20 compiler (clang/gcc)
- CMake, pkg-config
- All dependencies (Boost, spdlog, Kafka, etc.)
- Development tools (clang-tools, gdb, valgrind)
- Python with Zeek handler dependencies
- Zeek for network capture

### Build & Test

```bash
# Inside nix develop
cmake -B build -S cpp \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build -j

# Run tests
cd build && ctest --output-on-failure

# Run module
./build/src/inspector/inspector ../config.yaml
```

### Format Code

```bash
# Auto-provided in dev shell
clang-format -i cpp/src/**/*.{cpp,hpp}
```

## Benefits Over Docker

✅ **Reproducible Builds**
- Exact dependency versions
- Same build on any platform
- No "works on my machine"

✅ **Cross-Platform**
- Build for Linux from macOS
- Build for ARM from x86
- Single flake.nix for all platforms

✅ **Fast Iteration**
- Nix cache reuses builds
- Incremental compilation
- No image rebuild delays

✅ **Native Performance**
- No containerization overhead
- Direct system calls
- Full CPU access

✅ **Development Environment**
- Instant dev shell
- All tools included
- Consistent across team

## CI/CD Integration

### GitHub Actions

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: cachix/install-nix-action@v22
      - uses: cachix/cachix-action@v12
        with:
          name: hamstring
      
      - name: Build all modules
        run: nix build .#default
      
      - name: Run tests
        run: nix develop -c ctest --test-dir build
```

### Binary Cache

```bash
# Setup Cachix for faster builds
cachix use hamstring

# Build and push
nix build
cachix push hamstring ./result
```

## Migrating from Docker

### Docker Compose → Nix

**Before (docker-compose.yml):**
```yaml
services:
  logserver:
    build: ./cpp
    command: ./logserver config.yaml
```

**After (flake.nix):**
```nix
packages.logserver = buildModule { name = "logserver"; };
```

### Running Services

**Before:**
```bash
docker-compose up -d logserver
```

**After:**
```bash
# Development
nix run .#logserver -- config.yaml

# Production (NixOS)
systemctl start hamstring-logserver
```

## Troubleshooting

### Build Fails

```bash
# Clean build
nix build .#logserver --rebuild

# Verbose output
nix build .#logserver -L

# Show build logs
nix log .#logserver
```

### Missing Dependencies

```bash
# Update flake inputs
nix flake update

# Check what's available
nix flake show
```

### Platform-Specific Issues

```bash
# Force specific platform
nix build .#logserver \
  --system x86_64-linux \
  --option sandbox false
```

## Advanced Usage

### Custom Build Options

```nix
# In flake.nix
buildModule = { name, cmakeFlags ? [] }:
  pkgs.stdenv.mkDerivation {
    cmakeFlags = [
      "-DCMAKE_BUILD_TYPE=Release"
      "-DENABLE_ASAN=ON"  # AddressSanitizer
    ] ++ cmakeFlags;
  };
```

### Development with Different Compilers

```bash
# Use GCC
nix develop --override-input nixpkgs github:NixOS/nixpkgs/gcc-latest

# Use Clang
nix develop --override-input nixpkgs github:NixOS/nixpkgs/llvm-latest
```

## Summary

**Nix provides:**
- 🔄 Reproducible builds across platforms
- ⚡ Fast development iteration
- 📦 Declarative deployment
- 🔒 Hermetic build environment
- 🚀 Native performance (no containers)

**Perfect for:**
- Cross-platform C++ development
- CI/CD pipelines
- Production NixOS deployments
- Team development consistency
