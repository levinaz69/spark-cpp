// Spark C++ — SOGS Example
// Port of sparkjsdev/spark examples/sogs
// Loads a SOGS (.zip) or any supported splat file with orbit controls,
// sky gradient background, and ImGui control panel.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>

#include "render/spark_renderer.h"
#include "scene/splat_loader.h"
#include "scene/splat_generator.h"

// ─── Orbit Camera ───────────────────────────────────────────────────────────

struct OrbitCamera {
    glm::vec3 target{0.0f, 1.5f, 0.0f};
    float distance = 3.0f;
    float yaw   = 0.0f;   // radians, around Y
    float pitch  = 0.3f;   // radians, up from XZ plane
    float fov    = 60.0f;
    float near_plane = 0.01f;
    float far_plane  = 1000.0f;
    float min_distance = 0.2f;
    float max_distance = 50.0f;

    glm::vec3 position() const {
        float x = target.x + distance * cosf(pitch) * sinf(yaw);
        float y = target.y + distance * sinf(pitch);
        float z = target.z + distance * cosf(pitch) * cosf(yaw);
        return {x, y, z};
    }

    glm::mat4 view_matrix() const {
        return glm::lookAt(position(), target, glm::vec3(0, 1, 0));
    }

    glm::mat4 projection_matrix(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
    }

    void rotate(float dyaw, float dpitch) {
        yaw   += dyaw;
        pitch  = glm::clamp(pitch + dpitch, -1.5f, 1.5f);
    }

    void zoom(float delta) {
        distance = glm::clamp(distance * (1.0f - delta * 0.1f), min_distance, max_distance);
    }

    void pan(float dx, float dy) {
        glm::vec3 fwd = glm::normalize(target - position());
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::cross(right, fwd);
        float scale = distance * 0.002f;
        target += right * (-dx * scale) + up * (dy * scale);
    }

    // Fill spark::Camera from orbit state
    void fill(spark::Camera& cam, float aspect) const {
        cam.position = position();
        cam.fov = fov;
        cam.aspect = aspect;
        cam.near_plane = near_plane;
        cam.far_plane = far_plane;

        // The renderer uses camera.rotation (world-space orientation quaternion)
        // and camera.projection. Extract rotation from the lookAt direction.
        glm::vec3 dir = glm::normalize(target - cam.position);
        glm::vec3 world_up(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(dir, world_up));
        glm::vec3 up = glm::cross(right, dir);
        // Build a rotation matrix (camera looks along -Z in its local frame)
        glm::mat3 rot_mat;
        rot_mat[0] = right;
        rot_mat[1] = up;
        rot_mat[2] = -dir;
        cam.rotation = glm::quat_cast(rot_mat);

        cam.update_projection();
        cam.update_view();
    }
};

// ─── Sky Gradient Shader ────────────────────────────────────────────────────

static const char* sky_vert_src = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.9999, 1.0);
}
)";

static const char* sky_frag_src = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
uniform vec3 uSunDir;
void main() {
    float t = vUV.y;
    vec3 sky = mix(uBottomColor, uTopColor, t);
    // Sun glow
    vec2 sunUV = vec2(0.7, 0.75);
    float sunDist = length(vUV - sunUV);
    float glow = exp(-sunDist * sunDist * 20.0) * 0.6;
    sky += vec3(1.0, 0.95, 0.8) * glow;
    FragColor = vec4(sky, 1.0);
}
)";

struct SkyRenderer {
    GLuint program = 0;
    GLuint vao = 0, vbo = 0;
    glm::vec3 top_color{0.25f, 0.55f, 0.95f};
    glm::vec3 bottom_color{0.85f, 0.90f, 0.95f};
    glm::vec3 sun_dir{0.5f, 0.3f, 0.8f};

    bool init() {
        // Compile shaders
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &sky_vert_src, nullptr);
        glCompileShader(vs);
        GLint ok;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(vs, 512, nullptr, log);
            std::cerr << "Sky vertex shader error: " << log << std::endl;
            return false;
        }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &sky_frag_src, nullptr);
        glCompileShader(fs);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(fs, 512, nullptr, log);
            std::cerr << "Sky fragment shader error: " << log << std::endl;
            return false;
        }

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        // Fullscreen quad
        float quad[] = {-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1};
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        return true;
    }

    void render() {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(program);
        glUniform3fv(glGetUniformLocation(program, "uTopColor"), 1, glm::value_ptr(top_color));
        glUniform3fv(glGetUniformLocation(program, "uBottomColor"), 1, glm::value_ptr(bottom_color));
        glUniform3fv(glGetUniformLocation(program, "uSunDir"), 1, glm::value_ptr(sun_dir));
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void destroy() {
        if (program) glDeleteProgram(program);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
    }
};

// ─── App State ──────────────────────────────────────────────────────────────

static int g_width = 1280;
static int g_height = 720;
static bool g_mouse_dragging = false;
static bool g_middle_dragging = false;
static double g_last_mx = 0, g_last_my = 0;
static float g_scroll_accum = 0;

static void framebuffer_size_cb(GLFWwindow*, int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}

static void scroll_cb(GLFWwindow*, double, double yoff) {
    if (!ImGui::GetIO().WantCaptureMouse)
        g_scroll_accum += static_cast<float>(yoff);
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    std::string file_path;
    std::string shader_dir;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--shader-dir" && i + 1 < argc) shader_dir = argv[++i];
        else if (file_path.empty() && arg[0] != '-') file_path = arg;
    }

    // ── GLFW init ──
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(g_width, g_height, "Spark \xC2\xB7 SOGS Example", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetScrollCallback(window, scroll_cb);
    glfwSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { glfwTerminate(); return 1; }
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";

    // ── ImGui init ──
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    // ── Find shaders ──
    if (shader_dir.empty()) {
        const char* candidates[] = {
            "src/shader/glsl", "../src/shader/glsl", "../../src/shader/glsl",
        };
        for (auto p : candidates) {
            std::ifstream t(std::string(p) + "/splat_vertex.glsl");
            if (t.good()) { shader_dir = p; break; }
        }
    }
    if (shader_dir.empty()) {
        std::cerr << "Could not find shader directory. Use --shader-dir\n";
        glfwTerminate(); return 1;
    }

    // ── Renderer ──
    spark::SparkRenderer renderer;
    if (!renderer.init(shader_dir)) {
        std::cerr << "Renderer init failed\n"; glfwTerminate(); return 1;
    }

    SkyRenderer sky;
    if (!sky.init()) {
        std::cerr << "Sky init failed\n"; glfwTerminate(); return 1;
    }

    // ── Load splats ──
    std::unique_ptr<spark::PackedSplats> splats;
    if (!file_path.empty()) {
        std::cout << "Loading: " << file_path << "\n";
        splats = spark::SplatLoader::load_file(file_path);
    }
    if (splats && splats->num_splats() > 0) {
        renderer.set_splats(splats.get());
    }

    OrbitCamera orbit;
    spark::Camera cam;

    // ImGui state
    char load_path_buf[512] = {};
    if (!file_path.empty())
        snprintf(load_path_buf, sizeof(load_path_buf), "%s", file_path.c_str());
    bool show_sky = true;
    bool show_info = true;
    int fps = 0;
    float fps_timer = 0;
    int frame_count = 0;

    auto last_time = std::chrono::high_resolution_clock::now();

    // ── Main loop ──
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        // ── Mouse orbit controls (skip if ImGui wants mouse) ──
        if (!io.WantCaptureMouse) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            // Left-click drag → orbit
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                if (!g_mouse_dragging) {
                    g_mouse_dragging = true;
                    g_last_mx = mx; g_last_my = my;
                }
                float dx = static_cast<float>(mx - g_last_mx);
                float dy = static_cast<float>(my - g_last_my);
                orbit.rotate(-dx * 0.005f, dy * 0.005f);
                g_last_mx = mx; g_last_my = my;
            } else {
                g_mouse_dragging = false;
            }

            // Middle-click drag → pan
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
                if (!g_middle_dragging) {
                    g_middle_dragging = true;
                    g_last_mx = mx; g_last_my = my;
                }
                float dx = static_cast<float>(mx - g_last_mx);
                float dy = static_cast<float>(my - g_last_my);
                orbit.pan(dx, dy);
                g_last_mx = mx; g_last_my = my;
            } else {
                g_middle_dragging = false;
            }

            // Right-click drag → also orbit (alternative)
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                if (!g_mouse_dragging) {
                    g_mouse_dragging = true;
                    g_last_mx = mx; g_last_my = my;
                }
                float dx = static_cast<float>(mx - g_last_mx);
                float dy = static_cast<float>(my - g_last_my);
                orbit.rotate(-dx * 0.005f, dy * 0.005f);
                g_last_mx = mx; g_last_my = my;
            }

            // Scroll → zoom
            if (g_scroll_accum != 0) {
                orbit.zoom(g_scroll_accum);
                g_scroll_accum = 0;
            }
        }

        // ── Camera ──
        float aspect = static_cast<float>(g_width) / static_cast<float>(g_height);
        orbit.fill(cam, aspect);

        // ── Sort & Render ──
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (show_sky) sky.render();

        if (splats && splats->num_splats() > 0) {
            renderer.sort(cam);
            renderer.render(cam, g_width, g_height);
        }

        // ── ImGui ──
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Spark \xC2\xB7 SOGS Viewer")) {
            // File loading
            ImGui::SeparatorText("Load Splat File");
            ImGui::InputText("Path", load_path_buf, sizeof(load_path_buf));
            if (ImGui::Button("Load File")) {
                std::string path(load_path_buf);
                if (!path.empty()) {
                    std::cout << "Loading: " << path << "\n";
                    auto new_splats = spark::SplatLoader::load_file(path);
                    if (new_splats && new_splats->num_splats() > 0) {
                        splats = std::move(new_splats);
                        renderer.set_splats(splats.get());
                    } else {
                        std::cerr << "Failed to load: " << path << "\n";
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Generate Sphere")) {
                std::vector<uint32_t> packed;
                spark::SplatEncoding enc;
                spark::SplatGenerator::sphere(packed, enc);
                splats = std::make_unique<spark::PackedSplats>();
                splats->set_data(packed.data(), packed.size() / 4, enc);
                renderer.set_splats(splats.get());
            }

            // Info
            if (show_info) {
                ImGui::SeparatorText("Info");
                ImGui::Text("Splats: %zu", splats ? splats->num_splats() : 0);
                ImGui::Text("FPS: %d", fps);
                ImGui::Text("Camera: (%.1f, %.1f, %.1f)",
                    orbit.position().x, orbit.position().y, orbit.position().z);
                ImGui::Text("Target: (%.1f, %.1f, %.1f)",
                    orbit.target.x, orbit.target.y, orbit.target.z);
                ImGui::Text("Distance: %.2f", orbit.distance);
            }

            // Camera controls
            ImGui::SeparatorText("Camera");
            ImGui::DragFloat3("Target", glm::value_ptr(orbit.target), 0.1f);
            ImGui::SliderFloat("Distance", &orbit.distance, orbit.min_distance, orbit.max_distance);
            ImGui::SliderFloat("FOV", &orbit.fov, 20.0f, 120.0f);
            if (ImGui::Button("Reset Camera")) {
                orbit = OrbitCamera{};
            }

            // Sky
            ImGui::SeparatorText("Sky");
            ImGui::Checkbox("Show Sky", &show_sky);
            if (show_sky) {
                ImGui::ColorEdit3("Top", glm::value_ptr(sky.top_color));
                ImGui::ColorEdit3("Bottom", glm::value_ptr(sky.bottom_color));
            }

            // Renderer config
            ImGui::SeparatorText("Renderer");
            ImGui::SliderFloat("Max StdDev", &renderer.config.max_std_dev, 0.5f, 8.0f);
            ImGui::SliderFloat("Min Alpha", &renderer.config.min_alpha, 0.0f, 0.1f, "%.4f");
            ImGui::SliderFloat("Falloff", &renderer.config.falloff, 0.1f, 3.0f);
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        // FPS
        frame_count++;
        fps_timer += dt;
        if (fps_timer >= 1.0f) {
            fps = frame_count;
            std::string title = "Spark \xC2\xB7 SOGS - "
                + std::to_string(splats ? splats->num_splats() : 0)
                + " splats @ " + std::to_string(fps) + " FPS";
            glfwSetWindowTitle(window, title.c_str());
            frame_count = 0;
            fps_timer = 0;
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    sky.destroy();
    glfwTerminate();
    return 0;
}
