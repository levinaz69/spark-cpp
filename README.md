# Spark C++ — 3D Gaussian Splatting Renderer

C++ port of [sparkjsdev/spark](https://github.com/sparkjsdev/spark), a high-performance 3D Gaussian Splatting (3DGS) renderer.

Replaces the original TypeScript/THREE.js viewer with native **OpenGL 4.3 + GLFW**, and ports the Rust/WASM backend to native C++.

## Features

- **Gaussian Splatting rendering** via instanced quads with oriented 2D Gaussian projection
- **File format support**: `.splat` (antimatter15), `.ply` (standard 3DGS), more formats planned
- **CPU radix sort** (16-bit and 32-bit) for back-to-front depth ordering
- **Camera controls**: WASD/arrows movement, right-click mouse look, scroll zoom
- **Packed splat encoding**: 16 bytes per splat with half-float centers, octahedral quaternion encoding
- **Ray-ellipsoid intersection** for picking

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+
- OpenGL 4.3+ capable GPU

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

```bash
./spark_viewer <file.splat|file.ply> [options]
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `--width <int>` | Window width | 1280 |
| `--height <int>` | Window height | 720 |
| `--fov <float>` | Field of view (degrees) | 60 |
| `--shader-dir <path>` | GLSL shader directory | auto-detect |

### Controls

| Input | Action |
|-------|--------|
| WASD / Arrow keys | Move camera |
| Q / Space | Move up |
| E / Ctrl | Move down |
| Right-click drag | Rotate camera |
| Scroll wheel | Zoom |
| Shift | Move faster |
| Escape | Quit |

## Project Structure

```
src/
├── core/           # Core data structures (Gsplat, encoding, defines)
├── formats/        # File format decoders (PLY, .splat)
├── sort/           # Radix sort algorithms
├── raycast/        # Ray-ellipsoid intersection
├── render/         # OpenGL rendering pipeline
├── shader/         # GLSL shaders + shader manager
├── controls/       # Camera controls (GLFW input)
├── scene/          # Scene management (SplatLoader)
└── viewer/         # Main application (GLFW window)
```

## Architecture

This project ports the original Spark architecture from Rust+TypeScript+WebGL2 to pure C++:

| Original | C++ Port |
|----------|----------|
| THREE.js WebGL2 | OpenGL 4.3 Core |
| Browser Canvas | GLFW 3.x |
| glam (Rust) + THREE.js math | GLM |
| Web Workers + WASM | std::thread |
| half crate (Rust) | Native half-float utils |
| fflate/miniz_oxide | miniz |

## License

Based on [sparkjsdev/spark](https://github.com/sparkjsdev/spark).
