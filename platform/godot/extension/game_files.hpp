#pragma once
#include "game_src/interfaces/host.hpp"
#include <godot_cpp/variant/string.hpp>
#include <map>

namespace roadwars {
// Host-only discovery and case-aware access to a borrowed original installation.
godot::String resolve_game_path(const godot::String& root, const godot::String& relative,
                                bool directory, godot::String& error);
bool validate_game_directory(const godot::String& root, godot::String& error);
godot::String detect_game_directory();
godot::String current_game_directory();
class GameFiles final : public ResourceReader {
  public:
    explicit GameFiles(godot::String root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override;

  private:
    godot::String root_;
    std::map<std::string, godot::String> resolved_;
};
} // namespace roadwars
