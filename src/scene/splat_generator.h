#pragma once

#include <vector>
#include <cstdint>
#include "core/defines.h"

namespace spark {

// SplatGenerator: procedural Gaussian splat generators
class SplatGenerator {
public:
    // Generate a grid of splats
    static void grid(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                     int nx = 10, int ny = 10, int nz = 1,
                     float spacing = 0.1f, float scale = 0.02f);

    // Generate coordinate axes indicators
    static void axes(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                     float length = 1.0f, float scale = 0.01f, int points_per_axis = 50);

    // Generate a sphere of splats
    static void sphere(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                       float radius = 1.0f, float scale = 0.02f,
                       int num_points = 1000);

    // Generate snowfall-like particles
    static void snow(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                     float area = 10.0f, float height = 5.0f,
                     float scale = 0.005f, int num_particles = 5000);

    // Generate a point cloud cube
    static void cube(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                     float size = 1.0f, float scale = 0.015f,
                     int points_per_side = 10);

    // Generate a ground plane
    static void ground_plane(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                              float size = 10.0f, float scale = 0.05f,
                              int resolution = 50);
};

} // namespace spark
