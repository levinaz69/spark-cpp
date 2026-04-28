#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

#include "render/spark_renderer.h"
#include "controls/spark_controls.h"
#include "scene/splat_loader.h"
#include "scene/splat_generator.h"

static int g_width = 1280;
static int g_height = 720;

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    g_width = width;
    g_height = height;
    glViewport(0, 0, width, height);
}

static void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " <splat_file|--generate TYPE> [options]\n"
              << "\nSupported formats: .splat, .ply, .spz, .ksplat, .rad, .sogs\n"
              << "\nOptions:\n"
              << "  --width  <int>   Window width  (default: 1280)\n"
              << "  --height <int>   Window height (default: 720)\n"
              << "  --fov    <float> Field of view  (default: 60)\n"
              << "  --shader-dir <path> Shader directory\n"
              << "  --generate <type> Generate procedural splats:\n"
              << "     grid, axes, sphere, snow, cube, ground\n"
              << "\nControls:\n"
              << "  WASD / Arrow keys  Move camera\n"
              << "  Q / Space          Move up\n"
              << "  E / Ctrl           Move down\n"
              << "  Right-click drag   Rotate camera\n"
              << "  Scroll wheel       Zoom\n"
              << "  Shift              Move faster\n"
              << "  Escape             Quit\n";
}

int main(int argc, char** argv) {
    std::string splat_file;
    std::string generate_type;
    float fov = 60.0f;
    std::string shader_dir;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc) g_width = std::stoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc) g_height = std::stoi(argv[++i]);
        else if (arg == "--fov" && i + 1 < argc) fov = std::stof(argv[++i]);
        else if (arg == "--shader-dir" && i + 1 < argc) shader_dir = argv[++i];
        else if (arg == "--generate" && i + 1 < argc) generate_type = argv[++i];
        else if (splat_file.empty() && arg[0] != '-') splat_file = arg;
    }

    if (splat_file.empty() && generate_type.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(g_width, g_height, "Spark - Gaussian Splatting Viewer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1); // VSync

    // Initialize GLAD
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Find shader directory
    if (shader_dir.empty()) {
        // Try relative paths
        const char* candidates[] = {
            "src/shader/glsl",
            "../src/shader/glsl",
            "../../src/shader/glsl",
        };
        for (auto path : candidates) {
            std::ifstream test(std::string(path) + "/splat_vertex.glsl");
            if (test.good()) { shader_dir = path; break; }
        }
        if (shader_dir.empty()) {
            std::cerr << "Could not find shader directory. Use --shader-dir" << std::endl;
            glfwTerminate();
            return 1;
        }
    }

    // Initialize renderer
    spark::SparkRenderer renderer;
    if (!renderer.init(shader_dir)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        glfwTerminate();
        return 1;
    }

    // Load or generate splats
    auto splats = std::make_unique<spark::PackedSplats>();

    if (!generate_type.empty()) {
        std::cout << "Generating: " << generate_type << std::endl;
        std::vector<uint32_t> packed;
        spark::SplatEncoding enc;

        if (generate_type == "grid") spark::SplatGenerator::grid(packed, enc);
        else if (generate_type == "axes") spark::SplatGenerator::axes(packed, enc);
        else if (generate_type == "sphere") spark::SplatGenerator::sphere(packed, enc);
        else if (generate_type == "snow") spark::SplatGenerator::snow(packed, enc);
        else if (generate_type == "cube") spark::SplatGenerator::cube(packed, enc);
        else if (generate_type == "ground") spark::SplatGenerator::ground_plane(packed, enc);
        else {
            std::cerr << "Unknown generator: " << generate_type << std::endl;
            glfwTerminate();
            return 1;
        }

        if (!packed.empty()) {
            splats->set_data(packed.data(), packed.size() / 4, enc);
        }
    } else {
        std::cout << "Loading: " << splat_file << std::endl;
        splats = spark::SplatLoader::load_file(splat_file);
    }

    if (!splats || splats->num_splats() == 0) {
        std::cerr << "Failed to load/generate splats" << std::endl;
        glfwTerminate();
        return 1;
    }

    renderer.set_splats(splats.get());

    // Setup camera
    spark::Camera camera;
    camera.fov = fov;
    camera.position = glm::vec3(0.0f, 0.0f, 5.0f);

    // Setup controls
    spark::SparkControls controls;
    controls.attach(window);

    // Main loop
    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps_timer = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Escape to quit
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Delta time
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        // Update controls
        controls.update(dt);

        // Update camera
        camera.position = controls.position;
        camera.rotation = controls.rotation;
        camera.aspect = static_cast<float>(g_width) / static_cast<float>(g_height);
        camera.update_projection();
        camera.update_view();

        // Sort splats
        renderer.sort(camera);

        // Render
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer.render(camera, g_width, g_height);

        glfwSwapBuffers(window);

        // FPS counter
        frame_count++;
        fps_timer += dt;
        if (fps_timer >= 1.0f) {
            std::string title = "Spark - " + std::to_string(splats->num_splats())
                              + " splats @ " + std::to_string(frame_count) + " FPS";
            glfwSetWindowTitle(window, title.c_str());
            frame_count = 0;
            fps_timer = 0.0f;
        }
    }

    glfwTerminate();
    return 0;
}
