#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace spark {

class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    // Load shader from file
    std::string load_file(const std::string& path);

    // Compile shader with automatic #include resolution
    GLuint compile(GLenum type, const std::string& source, const std::string& name = "");

    // Link a vertex + fragment shader program
    GLuint link_program(GLuint vert, GLuint frag, const std::string& name = "");

    // Create a program from source strings
    GLuint create_program(const std::string& vert_source,
                          const std::string& frag_source,
                          const std::string& name = "");

    // Register include content
    void register_include(const std::string& name, const std::string& content);

    // Get uniform location (cached)
    GLint get_uniform(GLuint program, const std::string& name);

private:
    std::string resolve_includes(const std::string& source);

    std::unordered_map<std::string, std::string> includes_;
    std::unordered_map<uint64_t, GLint> uniform_cache_;
    std::vector<GLuint> shaders_;
    std::vector<GLuint> programs_;
};

} // namespace spark
