#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace spark {

class SparkControls {
public:
    SparkControls() = default;

    void attach(GLFWwindow* window);
    void update(float dt);

    // Camera state (read by renderer)
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Movement speed
    float move_speed = 2.0f;
    float fast_speed_multiplier = 3.0f;
    float rotate_speed = 0.003f;
    float scroll_speed = 0.5f;

    // Euler angles (pitch, yaw) in radians
    float pitch = 0.0f;
    float yaw = 0.0f;

private:
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    GLFWwindow* window_ = nullptr;

    bool keys_[512] = {};
    bool mouse_right_pressed_ = false;
    double last_mouse_x_ = 0, last_mouse_y_ = 0;
    bool first_mouse_ = true;
    float scroll_delta_ = 0.0f;
};

} // namespace spark
