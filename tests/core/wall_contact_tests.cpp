#include "game_src/unresolved/track_collision.hpp"
#include <cmath>
#include <iostream>
using namespace roadwars;
int main() {
    CollisionBody previous{{0, 1, 0}, 0, std::sqrt(2.0f), 45, 0, 2};
    CollisionBody current = previous;
    const std::array<WallRecord, 1> wall{{{{1.5f, -10, -10}, {1.5f, 10, 10}, {-1, 0, 0}, 20}}};
    bool enclosed{};
    if (body_intersects_walls(previous, wall, enclosed))
        return 1;
    current.position.x = 1;
    if (!body_intersects_walls(current, wall, enclosed) || enclosed)
        return 2;
    const auto result = solve_wall_contact(previous, current, wall);
    std::cout << "Wall contact hit=" << result.hit << " restore=" << result.restore_previous
              << " x=" << result.position.x << " normal=" << result.normal.x << ","
              << result.normal.y << "," << result.normal.z << " removed=" << result.removed_fraction
              << '\n';
    if (!result.hit || result.restore_previous || result.position.x >= 0.51f ||
        result.position.x < 0.48f || result.normal.x != -1)
        return 3;
    const std::array<WallRecord, 1> inside{{{{-0.2f, -10, 0}, {0.2f, 10, 0}, {0, 0, 1}, 0.4f}}};
    if (!body_intersects_walls(previous, inside, enclosed) || !enclosed)
        return 4;
    const auto no_hit = solve_wall_contact(previous, previous, wall);
    if (no_hit.hit || no_hit.restore_previous)
        return 5;
    return 0;
}
