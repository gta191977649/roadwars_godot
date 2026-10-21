#include "game_src/unresolved/weapon_mounts.hpp"
#include <stdexcept>
using namespace roadwars;
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Initial mount visibility mismatch");
}
int main() {
    MeshObject root;
    root.name = "Chassis";
    MeshObject body;
    body.name = "1024Body";
    for (const auto* name : {"ULRORMount1", "ULRORMount2", "ULRORMount3", "ULRORMount4",
                             "FrontWeaponMount", "RearWeaponMount"}) {
        MeshObject object;
        object.name = name;
        object.position = {1, 2, 3};
        body.children.push_back(object);
    }
    root.children.push_back(body);
    initialize_stock_weapon_mounts(root);
    for (std::size_t i = 0; i < root.children[0].children.size(); ++i) {
        const auto& object = root.children[0].children[i];
        require(object.visible == (i >= 3));
        require(object.position.x == 1 && object.position.y == 2 && object.position.z == 3);
    }
    require(root.visible && root.children[0].visible);
    initialize_stock_weapon_mounts(root);
    require(!root.children[0].children[0].visible);
    MeshObject no_mount;
    initialize_stock_weapon_mounts(no_mount);
    require(no_mount.visible);
}
