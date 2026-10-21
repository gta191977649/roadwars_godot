#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
namespace roadwars {
// Initial unarmed presentation only. Weapon deployment remains unavailable.
void initialize_stock_weapon_mounts(MeshObject& vehicle);
void hide_mesh_object(MeshObject& object);
} // namespace roadwars
