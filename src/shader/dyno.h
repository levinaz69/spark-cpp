#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <variant>

namespace spark {

// Dyno: Dynamic GPU shader graph system
// Builds GLSL shader code from a graph of connected nodes

// Data types for shader values
enum class DynoType {
    Float, Vec2, Vec3, Vec4,
    Int, IVec2, IVec3, IVec4,
    Mat3, Mat4, Bool, Sampler2D, Sampler2DArray,
};

// Node connection port
struct DynoPort {
    std::string name;
    DynoType type = DynoType::Float;
    std::string default_value;
};

// Base class for shader graph nodes
class DynoNode {
public:
    virtual ~DynoNode() = default;

    std::string name;
    std::vector<DynoPort> inputs;
    std::vector<DynoPort> outputs;

    // Generate GLSL code for this node
    virtual std::string generate(const std::vector<std::string>& input_vars) const = 0;

    // Type of the primary output
    virtual DynoType output_type() const { return outputs.empty() ? DynoType::Float : outputs[0].type; }
};

// Connection between nodes
struct DynoConnection {
    int from_node = -1;
    int from_port = 0;
    int to_node = -1;
    int to_port = 0;
};

// Math operation nodes
class DynoMathNode : public DynoNode {
public:
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        Abs, Floor, Ceil, Fract, Sign,
        Sqrt, Pow, Exp, Log,
        Sin, Cos, Tan, Asin, Acos, Atan,
        Min, Max, Clamp, Mix, Step, SmoothStep,
        Dot, Cross, Length, Normalize, Reflect,
    };

    Op op;
    DynoType result_type = DynoType::Float;

    DynoMathNode(Op operation, DynoType type = DynoType::Float);
    std::string generate(const std::vector<std::string>& input_vars) const override;
    DynoType output_type() const override { return result_type; }
};

// Constant/uniform value node
class DynoValueNode : public DynoNode {
public:
    DynoType value_type;
    std::string value_str;

    DynoValueNode(DynoType type, const std::string& value);
    std::string generate(const std::vector<std::string>& input_vars) const override;
    DynoType output_type() const override { return value_type; }
};

// Uniform node
class DynoUniformNode : public DynoNode {
public:
    std::string uniform_name;
    DynoType uniform_type;

    DynoUniformNode(const std::string& name, DynoType type);
    std::string generate(const std::vector<std::string>& input_vars) const override;
    DynoType output_type() const override { return uniform_type; }
};

// Texture sample node
class DynoTextureNode : public DynoNode {
public:
    std::string sampler_name;
    bool is_array = false;

    DynoTextureNode(const std::string& sampler, bool array_texture = false);
    std::string generate(const std::vector<std::string>& input_vars) const override;
    DynoType output_type() const override { return DynoType::Vec4; }
};

// Swizzle/component access node
class DynoSwizzleNode : public DynoNode {
public:
    std::string swizzle; // e.g., "xyz", "w", "xyzw"

    DynoSwizzleNode(const std::string& swizzle_str);
    std::string generate(const std::vector<std::string>& input_vars) const override;
    DynoType output_type() const override;
};

// Output node (writes to fragment color)
class DynoOutputNode : public DynoNode {
public:
    std::string output_name = "fragColor";

    DynoOutputNode(const std::string& name = "fragColor");
    std::string generate(const std::vector<std::string>& input_vars) const override;
};

// Branch/conditional node
class DynoBranchNode : public DynoNode {
public:
    DynoBranchNode();
    std::string generate(const std::vector<std::string>& input_vars) const override;
};

// DynoGraph: complete shader graph
class DynoGraph {
public:
    DynoGraph() = default;

    // Add nodes (returns node index)
    int add_node(std::shared_ptr<DynoNode> node);
    void connect(int from_node, int from_port, int to_node, int to_port);

    // Generate complete GLSL shader code
    std::string generate_vertex_shader() const;
    std::string generate_fragment_shader() const;
    std::string generate_code(bool is_fragment) const;

    // Uniforms discovered during code generation
    struct UniformInfo {
        std::string name;
        DynoType type;
    };
    const std::vector<UniformInfo>& uniforms() const { return uniforms_; }

    // Node access
    DynoNode* get_node(int index);
    const std::vector<std::shared_ptr<DynoNode>>& nodes() const { return nodes_; }
    const std::vector<DynoConnection>& connections() const { return connections_; }

private:
    std::vector<std::shared_ptr<DynoNode>> nodes_;
    std::vector<DynoConnection> connections_;
    mutable std::vector<UniformInfo> uniforms_;
};

// Helper to get GLSL type string
std::string dyno_type_str(DynoType type);

} // namespace spark
