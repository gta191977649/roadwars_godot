#pragma once
#include "game_src/interfaces/host.hpp"
#include <godot_cpp/variant/string.hpp>

namespace roadwars {
// Borrowed original installation. The only root comes from launcher configuration.
class GameFiles final : public ResourceReader {
  public:
    explicit GameFiles(godot::String root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override;

  private:
    godot::String root_;
};
} // namespace roadwars
