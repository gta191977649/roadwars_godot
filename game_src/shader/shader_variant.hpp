#pragma once
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace roadwars::shaders {
// RenderingServer expects preprocessed source; Shader resource preprocessing
// does not run for direct shader_set_code calls.
inline std::string shader_variant(std::string_view source, const std::string& defines) {
    std::istringstream input{std::string(source)};
    std::string line, output;
    std::vector<bool> enabled{true};
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.starts_with("#ifdef ") || line.starts_with("#ifndef ")) {
            const bool inverse = line.starts_with("#ifndef ");
            const auto name = line.substr(inverse ? 8 : 7);
            const bool present = defines.find("#define " + name + "\n") != std::string::npos;
            enabled.push_back(enabled.back() && (present != inverse));
        } else if (line == "#else") {
            if (enabled.size() < 2)
                throw std::runtime_error("Invalid shader conditional");
            enabled.back() = enabled[enabled.size() - 2] && !enabled.back();
        } else if (line == "#endif") {
            if (enabled.size() < 2)
                throw std::runtime_error("Invalid shader conditional");
            enabled.pop_back();
        } else if (enabled.back()) {
            output += line + "\n";
        }
    }
    if (enabled.size() != 1)
        throw std::runtime_error("Unterminated shader conditional");
    return output;
}
} // namespace roadwars::shaders
