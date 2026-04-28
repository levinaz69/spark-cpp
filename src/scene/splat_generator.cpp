#include "splat_generator.h"
#include "core/splat_encoding.h"
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace spark {

void SplatGenerator::grid(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                           int nx, int ny, int nz, float spacing, float scale) {
    size_t count = static_cast<size_t>(nx) * ny * nz;
    packed.resize(count * 4);

    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    float half_x = (nx - 1) * spacing * 0.5f;
    float half_y = (ny - 1) * spacing * 0.5f;
    float half_z = (nz - 1) * spacing * 0.5f;

    size_t idx = 0;
    for (int z = 0; z < nz; z++) {
        for (int y = 0; y < ny; y++) {
            for (int x = 0; x < nx; x++) {
                glm::vec3 center(x * spacing - half_x, y * spacing - half_y, z * spacing - half_z);
                float r = static_cast<float>(x) / std::max(1, nx - 1);
                float g = static_cast<float>(y) / std::max(1, ny - 1);
                float b = static_cast<float>(z) / std::max(1, nz - 1);
                glm::vec3 rgb(r, g, b);
                glm::vec3 s(scale);
                encode_packed_splat(&packed[idx * 4], center, 1.0f, rgb, s, identity, encoding);
                idx++;
            }
        }
    }
}

void SplatGenerator::axes(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                           float length, float scale, int points_per_axis) {
    size_t count = static_cast<size_t>(points_per_axis) * 3;
    packed.resize(count * 4);

    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 s(scale);
    size_t idx = 0;

    // X axis (red)
    for (int i = 0; i < points_per_axis; i++) {
        float t = static_cast<float>(i) / (points_per_axis - 1) * length;
        glm::vec3 center(t, 0.0f, 0.0f);
        encode_packed_splat(&packed[idx * 4], center, 1.0f, glm::vec3(1, 0, 0), s, identity, encoding);
        idx++;
    }

    // Y axis (green)
    for (int i = 0; i < points_per_axis; i++) {
        float t = static_cast<float>(i) / (points_per_axis - 1) * length;
        glm::vec3 center(0.0f, t, 0.0f);
        encode_packed_splat(&packed[idx * 4], center, 1.0f, glm::vec3(0, 1, 0), s, identity, encoding);
        idx++;
    }

    // Z axis (blue)
    for (int i = 0; i < points_per_axis; i++) {
        float t = static_cast<float>(i) / (points_per_axis - 1) * length;
        glm::vec3 center(0.0f, 0.0f, t);
        encode_packed_splat(&packed[idx * 4], center, 1.0f, glm::vec3(0, 0, 1), s, identity, encoding);
        idx++;
    }
}

void SplatGenerator::sphere(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                              float radius, float scale, int num_points) {
    packed.resize(static_cast<size_t>(num_points) * 4);

    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 s(scale);

    // Fibonacci sphere distribution
    float golden_ratio = (1.0f + std::sqrt(5.0f)) / 2.0f;
    for (int i = 0; i < num_points; i++) {
        float theta = 2.0f * static_cast<float>(M_PI) * i / golden_ratio;
        float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / num_points);

        glm::vec3 center(
            radius * std::sin(phi) * std::cos(theta),
            radius * std::sin(phi) * std::sin(theta),
            radius * std::cos(phi)
        );

        // Color from normal
        glm::vec3 rgb = glm::normalize(center) * 0.5f + 0.5f;

        encode_packed_splat(&packed[i * 4], center, 1.0f, rgb, s, identity, encoding);
    }
}

void SplatGenerator::snow(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                            float area, float height, float scale, int num_particles) {
    packed.resize(static_cast<size_t>(num_particles) * 4);

    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (int i = 0; i < num_particles; i++) {
        glm::vec3 center(
            dist(rng) * area * 0.5f,
            dist(rng) * height * 0.5f + height * 0.5f,
            dist(rng) * area * 0.5f
        );
        float size = scale * (0.5f + std::abs(dist(rng)));
        glm::vec3 rgb(0.95f, 0.95f, 1.0f); // white-ish
        glm::vec3 s(size);
        encode_packed_splat(&packed[i * 4], center, 0.8f, rgb, s, identity, encoding);
    }
}

void SplatGenerator::cube(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                            float size, float scale, int points_per_side) {
    // Generate points on the surface of a cube
    std::vector<glm::vec3> points;
    float half = size * 0.5f;
    float step = size / std::max(1, points_per_side - 1);

    // 6 faces
    for (int face = 0; face < 6; face++) {
        for (int i = 0; i < points_per_side; i++) {
            for (int j = 0; j < points_per_side; j++) {
                float u = -half + i * step;
                float v = -half + j * step;
                glm::vec3 p;
                switch (face) {
                    case 0: p = {half, u, v}; break;
                    case 1: p = {-half, u, v}; break;
                    case 2: p = {u, half, v}; break;
                    case 3: p = {u, -half, v}; break;
                    case 4: p = {u, v, half}; break;
                    case 5: p = {u, v, -half}; break;
                }
                points.push_back(p);
            }
        }
    }

    packed.resize(points.size() * 4);
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 s(scale);

    for (size_t i = 0; i < points.size(); i++) {
        glm::vec3 rgb = points[i] / size + 0.5f;
        encode_packed_splat(&packed[i * 4], points[i], 1.0f, rgb, s, identity, encoding);
    }
}

void SplatGenerator::ground_plane(std::vector<uint32_t>& packed, const SplatEncoding& encoding,
                                    float size, float scale, int resolution) {
    size_t count = static_cast<size_t>(resolution) * resolution;
    packed.resize(count * 4);

    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    float half = size * 0.5f;
    float step = size / (resolution - 1);

    size_t idx = 0;
    for (int z = 0; z < resolution; z++) {
        for (int x = 0; x < resolution; x++) {
            glm::vec3 center(-half + x * step, 0.0f, -half + z * step);
            // Checkerboard pattern
            bool check = ((x / 5) + (z / 5)) % 2 == 0;
            glm::vec3 rgb = check ? glm::vec3(0.4f, 0.4f, 0.4f) : glm::vec3(0.6f, 0.6f, 0.6f);
            glm::vec3 s(scale, scale * 0.1f, scale); // flat
            encode_packed_splat(&packed[idx * 4], center, 0.9f, rgb, s, identity, encoding);
            idx++;
        }
    }
}

} // namespace spark
