# Docker to NixOS Migration Guide

## Overview

HAMSTRING now uses **Nix** instead of Docker for reproducible,cross-platform builds. OCI images are generated from Nix for Docker compatibility.

## What Changed

### Before (Docker)
```bash
docker build -f docker/dockerfiles/Dockerfile.logserver -t hamstring/logserver .
docker run hamstring/logserver
```

### After (Nix)
```bash
nix build .#oci-logserver
docker load < result
docker run hamstring/logserver:latest
```

## Available OCI Images

| Image | Nix Package | Description |
|-------|-------------|-------------|
| `hamstring/logserver` | `.#oci-logserver` | C++ LogServer |
| `hamstring/logcollector` | `.#oci-logcollector` | C++ LogCollector |
| `hamstring/prefilter` | `.#oci-prefilter` | C++ Prefilter |
| `hamstring/inspector` | `.#oci-inspector` | C++ ML Inspector |
| `hamstring/zeek` | `.#oci-zeek` | Zeek network capture |

## Build OCI Images

### Single Image
```bash
# Build
nix build .#oci-inspector

# Load into Docker
docker load < result

# Run
docker run -v $(pwd)/config.yaml:/config.yaml \
  hamstring/inspector:latest
```

### All Images
```bash
# Build all OCI images
nix build .#oci-images

# Load all at once
for img in result-*; do
  docker load < $img
done

# List images
docker images | grep hamstring
```

## Cross-Platform Build

```bash
# Build for Linux (from macOS)
nix build .#oci-logserver --system x86_64-linux

# Build for ARM
nix build .#oci-logserver --system aarch64-linux

# Build for all platforms
nix build .#oci-logserver \
  --system x86_64-linux \
  --system aarch64-darwin
```

## Docker Compose Equivalent

### Old docker-compose.yml
```yaml
services:
  logserver:
    build:
      context: .
      dockerfile: docker/dockerfiles/Dockerfile.logserver
    command: ./logserver config.yaml
```

### New (Nix-built images)
```yaml
services:
  logserver:
    image: hamstring/logserver:latest
    volumes:
      - ./config.yaml:/config.yaml
```

## Testing

```bash
# Start Kafka
docker run -d --name kafka apache/kafka:latest

# Run pipeline modules
docker run --link kafka -v $(pwd)/config.yaml:/config.yaml \
  hamstring/logserver:latest

docker run --link kafka -v $(pwd)/config.yaml:/config.yaml \
  hamstring/inspector:latest
```

## Benefits

✅ **Reproducible**: Same build everywhere  
✅ **Fast**: Nix caching  
✅ **Cross-platform**: Build for any architecture  
✅ **Pure**: Hermetic builds  
✅ **Small**: Minimal images (no layers waste)

## Migration Checklist

- [x] Convert C++ modules to Nix packages
- [x] Create OCI image builders  
- [x] Add Zeek image
- [x] Document migration
- [ ] Update CI/CD to use Nix
- [ ] Archive old Dockerfiles

## Advanced

### Custom OCI Image
```nix
# In flake.nix
oci-custom = buildOciImage {
  name = "mymodule";
  package = self.packages.${system}.mymodule;
};
```

### Push to Registry
```bash
# Build
nix build .#oci-inspector

# Push to Docker Hub
docker load < result
docker tag hamstring/inspector:latest myuser/inspector:v1.0
docker push myuser/inspector:v1.0

# Or use skopeo (no Docker daemon needed)
skopeo copy oci-archive:result docker://myuser/inspector:v1.0
```

## Summary

**Nix provides:**
- 🔄 Bit-for-bit reproducible builds
- 🌍 True cross-platform compilation  
- ⚡ Faster builds with caching
- 🐳 Docker-compatible OCI images
- 🔒 Hermetic, auditable builds

**Perfect for CI/CD and production deployments!**
