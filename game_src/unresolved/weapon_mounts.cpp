#include "game_src/unresolved/weapon_mounts.hpp"
namespace roadwars {
namespace {
MeshObject* find(MeshObject& root, std::string_view name) {
    if (root.name == name)
        return &root;
    for (auto& child : root.children)
        if (auto* result = find(child, name))
            return result;
    return nullptr;
}
} // namespace
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d28b0
void hide_mesh_object(MeshObject& object) { object.visible = false; }
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0040face
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004cfc3d
// Constructor visibility before first render; full ownership/callbacks remain unavailable.
void initialize_stock_weapon_mounts(MeshObject& vehicle) {
    for (int index = 1; index <= 3; ++index)
        if (auto* mount = find(vehicle, "ULRORMount" + std::to_string(index)))
            hide_mesh_object(*mount);
}
} // namespace roadwars
