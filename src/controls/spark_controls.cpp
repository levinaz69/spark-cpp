#include "spark_controls.h"
#include <cmath>
#include <algorithm>

namespace spark {

void SparkControls::attach(GLFWwindow* window) {
    window_ = window;
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
}

void SparkControls::update(float dt) {
    if (!window_) return;

    float speed = move_speed;
    if (keys_[GLFW_KEY_LEFT_SHIFT] || keys_[GLFW_KEY_RIGHT_SHIFT]) {
        speed *= fast_speed_multiplier;
    }

    // Movement
    glm::vec3 move_dir(0.0f);

    if (keys_[GLFW_KEY_W] || keys_[GLFW_KEY_UP])     move_dir.z -= 1.0f;
    if (keys_[GLFW_KEY_S] || keys_[GLFW_KEY_DOWN])    move_dir.z += 1.0f;
    if (keys_[GLFW_KEY_A] || keys_[GLFW_KEY_LEFT])    move_dir.x -= 1.0f;
    if (keys_[GLFW_KEY_D] || keys_[GLFW_KEY_RIGHT])   move_dir.x += 1.0f;
    if (keys_[GLFW_KEY_Q] || keys_[GLFW_KEY_SPACE])   move_dir.y += 1.0f;
    if (keys_[GLFW_KEY_E] || keys_[GLFW_KEY_LEFT_CONTROL]) move_dir.y -= 1.0f;

    if (glm::length(move_dir) > 0.0f) {
        move_dir = glm::normalize(move_dir);
        position += rotation * move_dir * speed * dt;
    }

    // Scroll: move forward/backward
    if (scroll_delta_ != 0.0f) {
        position += rotation * glm::vec3(0.0f, 0.0f, -scroll_delta_ * scroll_speed);
        scroll_delta_ = 0.0f;
    }

    // Update rotation from euler angles
    glm::quat pitch_q = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yaw_q = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    rotation = yaw_q * pitch_q;
}

void SparkControls::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = static_cast<SparkControls*>(glfwGetWindowUserPointer(window));
    if (!self || key < 0 || key >= 512) return;

    if (action == GLFW_PRESS) self->keys_[key] = true;
    else if (action == GLFW_RELEASE) self->keys_[key] = false;
}

void SparkControls::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<SparkControls*>(glfwGetWindowUserPointer(window));
    if (!self) return;

    if (self->first_mouse_) {
        self->last_mouse_x_ = xpos;
        self->last_mouse_y_ = ypos;
        self->first_mouse_ = false;
        return;
    }

    if (self->mouse_right_pressed_) {
        double dx = xpos - self->last_mouse_x_;
        double dy = ypos - self->last_mouse_y_;

        self->yaw -= static_cast<float>(dx) * self->rotate_speed;
        self->pitch -= static_cast<float>(dy) * self->rotate_speed;

        // Clamp pitch
        self->pitch = std::clamp(self->pitch, -1.5f, 1.5f);
    }

    self->last_mouse_x_ = xpos;
    self->last_mouse_y_ = ypos;
}

void SparkControls::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* self = static_cast<SparkControls*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    self->scroll_delta_ += static_cast<float>(yoffset);
}

void SparkControls::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<SparkControls*>(glfwGetWindowUserPointer(window));
    if (!self) return;

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        self->mouse_right_pressed_ = (action == GLFW_PRESS);
    }
}

} // namespace spark
