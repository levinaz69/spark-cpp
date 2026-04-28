# Spark C++ — 3D Gaussian Splatting Renderer

C++ port of [sparkjsdev/spark](https://github.com/sparkjsdev/spark), a high-performance 3D Gaussian Splatting (3DGS) renderer.

Replaces the original TypeScript/THREE.js viewer with native **OpenGL 4.3 + GLFW**, and ports the Rust/WASM backend to native C++.

## Features

- **Gaussian Splatting rendering** via instanced quads with oriented 2D Gaussian projection
- **File format support**: `.splat`, `.ply`, `.spz` (Niantic), `.ksplat`, `.rad` (Spark), `.sogs`/`.pcsogs` (ZIP)
- **CPU radix sort** (16-bit and 32-bit) for back-to-front depth ordering
- **LOD system**: LodTree (BFS traversal), QuickLod (scale-based hierarchy), TinyLod
- **Camera controls**: WASD/arrows movement, right-click mouse look, scroll zoom
- **Packed splat encoding**: 16 bytes per splat with half-float centers, octahedral quaternion encoding
- **Ray-ellipsoid intersection** for picking
- **SDF-based editing**: Sphere/Box/Cylinder/Plane region selection with delete/hide/tint/scale/move ops
- **Skeletal animation**: Dual-quaternion bone deformation (SplatSkinning)
- **Splat accumulation**: Weighted blending of multiple splat contributions
- **Portal system**: Teleportation between scenes/locations
- **LOD paging**: Distance-based streaming with page callbacks
- **Extended precision**: Float32 GPU texture centers for large scenes
- **SH clustering**: k-means++ spherical harmonics compression
- **Dyno shader graph**: Dynamic GLSL code generation from node graphs
- **GPU readback**: PBO-based async pixel/depth readback
- **Procedural generators**: Grid, axes, sphere, snow, cube, ground plane

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
./spark_viewer <file> [options]
./spark_viewer --generate <type> [options]
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `--width <int>` | Window width | 1280 |
| `--height <int>` | Window height | 720 |
| `--fov <float>` | Field of view (degrees) | 60 |
| `--shader-dir <path>` | GLSL shader directory | auto-detect |
| `--generate <type>` | Procedural: grid, axes, sphere, snow, cube, ground | — |

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
├── core/           # Core data structures (Gsplat, encoding, ExtSplats, SH clustering)
├── formats/        # File format decoders (PLY, .splat, SPZ, KSPLAT, RAD, SOGS)
├── sort/           # Radix sort algorithms
├── lod/            # LOD tree, QuickLod, TinyLod
├── raycast/        # Ray-ellipsoid intersection
├── render/         # OpenGL rendering pipeline, readback
├── shader/         # GLSL shaders, shader manager, Dyno graph
├── controls/       # Camera controls (GLFW input)
├── scene/          # Scene management, SplatMesh, SplatEdit, SplatSkinning,
│                   # SplatAccumulator, SplatPager, SparkPortals, generators
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
