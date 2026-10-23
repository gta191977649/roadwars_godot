#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <array>
#include <string>

namespace roadwars {
bool chrome_mapping_applies(bool enabled, std::uint32_t flags, std::size_t part_index,
                            bool texture_bound);
std::array<float, 2> chrome_mapping_uv(Vec3 normal, const Transform& object_to_view);
std::array<float, 3> chrome_mapping_add(std::array<float, 3> base,
                                      std::array<float, 3> chrome);
// Caller owns a returned texture and releases it after all referencing materials.
Result load_chrome_mapping(ResourceStore&, Renderer&, TextureHandle&, bool& enabled,
                            std::string& error);
} // namespace roadwars
