#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
namespace roadwars {
// Stock preview visibility; live equipment deployment is owned by VehicleCombat.
void initialize_stock_weapon_mounts(MeshObject& vehicle);
void hide_mesh_object(MeshObject& object);
} // namespace roadwars
