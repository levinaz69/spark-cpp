#include "dyno.h"
#include <algorithm>
#include <stdexcept>
#include <set>

namespace spark {

std::string dyno_type_str(DynoType type) {
    switch (type) {
        case DynoType::Float: return "float";
        case DynoType::Vec2: return "vec2";
        case DynoType::Vec3: return "vec3";
        case DynoType::Vec4: return "vec4";
        case DynoType::Int: return "int";
        case DynoType::IVec2: return "ivec2";
        case DynoType::IVec3: return "ivec3";
        case DynoType::IVec4: return "ivec4";
        case DynoType::Mat3: return "mat3";
        case DynoType::Mat4: return "mat4";
        case DynoType::Bool: return "bool";
        case DynoType::Sampler2D: return "sampler2D";
        case DynoType::Sampler2DArray: return "sampler2DArray";
    }
    return "float";
}

// DynoMathNode
DynoMathNode::DynoMathNode(Op operation, DynoType type)
    : op(operation), result_type(type) {
    name = "math";
    // Most math ops take 2 inputs
    inputs = {{"a", type}, {"b", type}};
    outputs = {{"result", type}};

    // Single-input ops
    switch (op) {
        case Op::Abs: case Op::Floor: case Op::Ceil: case Op::Fract:
        case Op::Sign: case Op::Sqrt: case Op::Exp: case Op::Log:
        case Op::Sin: case Op::Cos: case Op::Tan: case Op::Asin:
        case Op::Acos: case Op::Atan: case Op::Length: case Op::Normalize:
            inputs = {{"a", type}};
            break;
        case Op::Clamp: case Op::Mix: case Op::SmoothStep:
            inputs = {{"a", type}, {"b", type}, {"c", type}};
            break;
        default: break;
    }

    if (op == Op::Dot || op == Op::Length) {
        outputs = {{"result", DynoType::Float}};
        result_type = DynoType::Float;
    }
    if (op == Op::Cross) {
        result_type = DynoType::Vec3;
        outputs = {{"result", DynoType::Vec3}};
    }
}

std::string DynoMathNode::generate(const std::vector<std::string>& input_vars) const {
    auto a = input_vars.empty() ? "0.0" : input_vars[0];
    auto b = input_vars.size() > 1 ? input_vars[1] : "0.0";
    auto c = input_vars.size() > 2 ? input_vars[2] : "0.0";

    switch (op) {
        case Op::Add: return "(" + a + " + " + b + ")";
        case Op::Sub: return "(" + a + " - " + b + ")";
        case Op::Mul: return "(" + a + " * " + b + ")";
        case Op::Div: return "(" + a + " / " + b + ")";
        case Op::Mod: return "mod(" + a + ", " + b + ")";
        case Op::Abs: return "abs(" + a + ")";
        case Op::Floor: return "floor(" + a + ")";
        case Op::Ceil: return "ceil(" + a + ")";
        case Op::Fract: return "fract(" + a + ")";
        case Op::Sign: return "sign(" + a + ")";
        case Op::Sqrt: return "sqrt(" + a + ")";
        case Op::Pow: return "pow(" + a + ", " + b + ")";
        case Op::Exp: return "exp(" + a + ")";
        case Op::Log: return "log(" + a + ")";
        case Op::Sin: return "sin(" + a + ")";
        case Op::Cos: return "cos(" + a + ")";
        case Op::Tan: return "tan(" + a + ")";
        case Op::Asin: return "asin(" + a + ")";
        case Op::Acos: return "acos(" + a + ")";
        case Op::Atan: return "atan(" + a + ")";
        case Op::Min: return "min(" + a + ", " + b + ")";
        case Op::Max: return "max(" + a + ", " + b + ")";
        case Op::Clamp: return "clamp(" + a + ", " + b + ", " + c + ")";
        case Op::Mix: return "mix(" + a + ", " + b + ", " + c + ")";
        case Op::Step: return "step(" + a + ", " + b + ")";
        case Op::SmoothStep: return "smoothstep(" + a + ", " + b + ", " + c + ")";
        case Op::Dot: return "dot(" + a + ", " + b + ")";
        case Op::Cross: return "cross(" + a + ", " + b + ")";
        case Op::Length: return "length(" + a + ")";
        case Op::Normalize: return "normalize(" + a + ")";
        case Op::Reflect: return "reflect(" + a + ", " + b + ")";
    }
    return a;
}

// DynoValueNode
DynoValueNode::DynoValueNode(DynoType type, const std::string& value)
    : value_type(type), value_str(value) {
    name = "value";
    outputs = {{"value", type}};
}

std::string DynoValueNode::generate(const std::vector<std::string>&) const {
    return value_str;
}

// DynoUniformNode
DynoUniformNode::DynoUniformNode(const std::string& uname, DynoType type)
    : uniform_name(uname), uniform_type(type) {
    name = "uniform_" + uname;
    outputs = {{"value", type}};
}

std::string DynoUniformNode::generate(const std::vector<std::string>&) const {
    return uniform_name;
}

// DynoTextureNode
DynoTextureNode::DynoTextureNode(const std::string& sampler, bool array_texture)
    : sampler_name(sampler), is_array(array_texture) {
    name = "texture_" + sampler;
    inputs = {{"uv", is_array ? DynoType::Vec3 : DynoType::Vec2}};
    outputs = {{"color", DynoType::Vec4}};
}

std::string DynoTextureNode::generate(const std::vector<std::string>& input_vars) const {
    auto uv = input_vars.empty() ? "vec2(0.0)" : input_vars[0];
    return "texture(" + sampler_name + ", " + uv + ")";
}

// DynoSwizzleNode
DynoSwizzleNode::DynoSwizzleNode(const std::string& swizzle_str)
    : swizzle(swizzle_str) {
    name = "swizzle";
    inputs = {{"input", DynoType::Vec4}};
    outputs = {{"output", output_type()}};
}

std::string DynoSwizzleNode::generate(const std::vector<std::string>& input_vars) const {
    auto input = input_vars.empty() ? "vec4(0.0)" : input_vars[0];
    return input + "." + swizzle;
}

DynoType DynoSwizzleNode::output_type() const {
    switch (swizzle.size()) {
        case 1: return DynoType::Float;
        case 2: return DynoType::Vec2;
        case 3: return DynoType::Vec3;
        case 4: return DynoType::Vec4;
    }
    return DynoType::Float;
}

// DynoOutputNode
DynoOutputNode::DynoOutputNode(const std::string& name_str)
    : output_name(name_str) {
    name = "output";
    inputs = {{"color", DynoType::Vec4}};
}

std::string DynoOutputNode::generate(const std::vector<std::string>& input_vars) const {
    auto color = input_vars.empty() ? "vec4(1.0)" : input_vars[0];
    return output_name + " = " + color;
}

// DynoBranchNode
DynoBranchNode::DynoBranchNode() {
    name = "branch";
    inputs = {{"condition", DynoType::Bool}, {"true_val", DynoType::Vec4}, {"false_val", DynoType::Vec4}};
    outputs = {{"result", DynoType::Vec4}};
}

std::string DynoBranchNode::generate(const std::vector<std::string>& input_vars) const {
    auto cond = input_vars.empty() ? "true" : input_vars[0];
    auto t = input_vars.size() > 1 ? input_vars[1] : "vec4(1.0)";
    auto f = input_vars.size() > 2 ? input_vars[2] : "vec4(0.0)";
    return "(" + cond + " ? " + t + " : " + f + ")";
}

// DynoGraph
int DynoGraph::add_node(std::shared_ptr<DynoNode> node) {
    nodes_.push_back(std::move(node));
    return static_cast<int>(nodes_.size() - 1);
}

void DynoGraph::connect(int from_node, int from_port, int to_node, int to_port) {
    connections_.push_back({from_node, from_port, to_node, to_port});
}

DynoNode* DynoGraph::get_node(int index) {
    if (index >= 0 && index < static_cast<int>(nodes_.size()))
        return nodes_[index].get();
    return nullptr;
}

std::string DynoGraph::generate_code(bool is_fragment) const {
    // Topological sort
    int n = static_cast<int>(nodes_.size());
    std::vector<int> in_degree(n, 0);
    std::vector<std::vector<int>> adj(n);

    for (const auto& c : connections_) {
        in_degree[c.to_node]++;
        adj[c.from_node].push_back(c.to_node);
    }

    std::vector<int> order;
    std::vector<int> queue;
    for (int i = 0; i < n; i++) {
        if (in_degree[i] == 0) queue.push_back(i);
    }

    while (!queue.empty()) {
        int node = queue.back();
        queue.pop_back();
        order.push_back(node);
        for (int next : adj[node]) {
            if (--in_degree[next] == 0) queue.push_back(next);
        }
    }

    // Generate code
    std::ostringstream code;
    std::vector<std::string> node_vars(n);

    // Collect uniforms
    uniforms_.clear();
    for (int i : order) {
        auto* uniform = dynamic_cast<DynoUniformNode*>(nodes_[i].get());
        if (uniform) {
            uniforms_.push_back({uniform->uniform_name, uniform->uniform_type});
        }
    }

    // Generate uniform declarations
    std::set<std::string> declared_uniforms;
    for (const auto& u : uniforms_) {
        if (declared_uniforms.insert(u.name).second) {
            code << "uniform " << dyno_type_str(u.type) << " " << u.name << ";\n";
        }
    }

    code << "\nvoid dyno_main() {\n";

    for (int idx : order) {
        // Collect input vars for this node
        std::vector<std::string> input_vars;
        for (const auto& input : nodes_[idx]->inputs) {
            std::string var = input.default_value.empty() ? "0.0" : input.default_value;
            // Find connection to this input
            for (const auto& c : connections_) {
                if (c.to_node == idx) {
                    int port_idx = 0;
                    for (size_t p = 0; p < nodes_[idx]->inputs.size(); p++) {
                        if (static_cast<int>(p) == c.to_port) {
                            var = node_vars[c.from_node];
                            break;
                        }
                        port_idx++;
                    }
                }
            }
            input_vars.push_back(var);
        }

        std::string result = nodes_[idx]->generate(input_vars);
        std::string var_name = "dyno_v" + std::to_string(idx);
        node_vars[idx] = var_name;

        // Check if this is an output node
        auto* output = dynamic_cast<DynoOutputNode*>(nodes_[idx].get());
        if (output) {
            code << "    " << result << ";\n";
        } else {
            code << "    " << dyno_type_str(nodes_[idx]->output_type()) << " " << var_name << " = " << result << ";\n";
        }
    }

    code << "}\n";
    return code.str();
}

std::string DynoGraph::generate_vertex_shader() const {
    return generate_code(false);
}

std::string DynoGraph::generate_fragment_shader() const {
    return generate_code(true);
}

} // namespace spark
