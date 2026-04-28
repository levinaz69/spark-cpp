#include "shader_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

namespace spark {

ShaderManager::~ShaderManager() {
    for (auto s : shaders_) glDeleteShader(s);
    for (auto p : programs_) glDeleteProgram(p);
}

std::string ShaderManager::load_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ShaderManager: Failed to open " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void ShaderManager::register_include(const std::string& name, const std::string& content) {
    includes_[name] = content;
}

std::string ShaderManager::resolve_includes(const std::string& source) {
    std::string result;
    std::istringstream iss(source);
    std::string line;

    while (std::getline(iss, line)) {
        // Match: // #include "name" or // #include <name>
        size_t inc_pos = line.find("#include");
        if (inc_pos != std::string::npos) {
            // Extract include name
            auto start = line.find_first_of("\"<", inc_pos);
            auto end = line.find_first_of("\">", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                std::string name = line.substr(start + 1, end - start - 1);
                auto it = includes_.find(name);
                if (it != includes_.end()) {
                    result += "// BEGIN include: " + name + "\n";
                    result += it->second + "\n";
                    result += "// END include: " + name + "\n";
                    continue;
                }
            }
        }
        result += line + "\n";
    }

    return result;
}

GLuint ShaderManager::compile(GLenum type, const std::string& source, const std::string& name) {
    std::string resolved = resolve_includes(source);

    GLuint shader = glCreateShader(type);
    const char* src = resolved.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        std::cerr << "Shader compilation error (" << name << "):\n" << info_log << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    shaders_.push_back(shader);
    return shader;
}

GLuint ShaderManager::link_program(GLuint vert, GLuint frag, const std::string& name) {
    if (!vert || !frag) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        std::cerr << "Program link error (" << name << "):\n" << info_log << std::endl;
        glDeleteProgram(program);
        return 0;
    }

    programs_.push_back(program);
    return program;
}

GLuint ShaderManager::create_program(const std::string& vert_source,
                                      const std::string& frag_source,
                                      const std::string& name) {
    GLuint vert = compile(GL_VERTEX_SHADER, vert_source, name + ".vert");
    GLuint frag = compile(GL_FRAGMENT_SHADER, frag_source, name + ".frag");
    return link_program(vert, frag, name);
}

GLint ShaderManager::get_uniform(GLuint program, const std::string& name) {
    uint64_t key = (static_cast<uint64_t>(program) << 32) |
                   static_cast<uint64_t>(std::hash<std::string>{}(name));
    auto it = uniform_cache_.find(key);
    if (it != uniform_cache_.end()) return it->second;

    GLint loc = glGetUniformLocation(program, name.c_str());
    uniform_cache_[key] = loc;
    return loc;
}

} // namespace spark
