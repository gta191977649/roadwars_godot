#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/vehicle_dynamics.hpp"
#include "game_src/unresolved/wheel_pose.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Wheel pose contract failed");
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& bytes) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::unavailable;
        bytes.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        return file.read(reinterpret_cast<char*>(bytes.data()), size) ? Result::ok
                                                                      : Result::unavailable;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    std::array<float, 4> spins{};
    const std::array<float, 4> grounded{};
    require(advance_wheel_spin(spins, 10, 0, 2, 0, grounded, .1f) == Result::ok);
    require(spins[0] == -180 && spins[3] == -180);
    require(advance_wheel_spin(spins, 10, 90, 2, 0, grounded, .1f) == Result::ok);
    require(spins[0] == -180);
    require(advance_wheel_spin(spins, 10, 180, 2, 1, grounded, .1f) == Result::ok);
    require(spins[0] == -180);
    std::array<float, 4> airborne{1, 0, 0, 0};
    require(advance_wheel_spin(spins, 10, 180, 2, 0, airborne, .1f) == Result::ok);
    require(spins[0] == -180 && std::abs(spins[1]) < .001f);
    require(advance_wheel_spin(spins, 10, 0, 2, 0, grounded, .1f, false) == Result::ok);
    require(spins[0] == -180);
    require(advance_wheel_spin(spins, 10, 0, 0, 0, grounded, .1f) == Result::invalid_argument);
    std::array<float, 4> ai_spins{};
    require(advance_wheel_spin(ai_spins, 10, 0, 2, -.005f, grounded, .1f) == Result::ok);
    require(std::abs(ai_spins[0] - -180.9f) < .001f);
    require(advance_wheel_spin(ai_spins, 10, 0, 2, -.02f, grounded, .1f) ==
            Result::invalid_argument);
    const auto left = original_wheel_transform(0, {1, 2, 3}, 0, 0, 1.4f);
    const auto right = original_wheel_transform(1, {1, 2, 3}, 0, 0, 1.4f);
    require(left.origin.x == 1 && left.origin.y == 2 && left.origin.z == 3);
    require(left.basis[0].x == 1.4f && left.basis[1].y == 1 && left.basis[2].z == 1);
    require(right.basis[0].x == -1.4f && right.basis[1].y == 1 && right.basis[2].z == -1);
    const auto front = original_wheel_transform(0, {}, 30, 0, 1);
    const auto rear = original_wheel_transform(2, {}, 30, 0, 1);
    require(front.basis[2].x > .49f && rear.basis[2].x == 0);
    if (argc > 1) {
        Files files(argv[1]);
        ResourceStore resources(files);
        require(resources.open() == Result::ok);
        Configuration choices;
        require(choices.load(resources, "Screens/SelectCharacter/Data.ini") == Result::ok);
        float count{};
        require(choices.number("INFO", "NUMSELECTIONS", count) == Result::ok);
        for (int n = 0; n < static_cast<int>(count); ++n) {
            const auto car = choices.value("SELECTION" + std::to_string(n), "VALUE");
            VehicleParameters parameters;
            std::string error;
            require(load_vehicle_metrics(resources, car, parameters, error) == Result::ok);
            require(load_vehicle_geometry(resources, car, parameters, error) == Result::ok);
            std::vector<std::byte> bytes;
            MeshObject body;
            require(resources.read("Cars/" + car + "/Body/SpatialObject.msh", bytes) == Result::ok);
            require(decode_msh(bytes, body, error) == Result::ok);
            for (std::size_t wheel = 0; wheel < 4; ++wheel) {
                bool axle{}, ground{};
                for (const auto& child : body.children) {
                    if (child.name == "Tire" + std::to_string(wheel + 1)) {
                        require(parameters.wheel_axle_anchors[wheel].x == child.position.x &&
                                parameters.wheel_axle_anchors[wheel].y == child.position.y &&
                                parameters.wheel_axle_anchors[wheel].z == child.position.z);
                        axle = true;
                    }
                    if (child.name == "Tire" + std::to_string(wheel + 1) + "Ground") {
                        require(parameters.wheel_ground_anchors[wheel].x == child.position.x &&
                                parameters.wheel_ground_anchors[wheel].y == child.position.y &&
                                parameters.wheel_ground_anchors[wheel].z == child.position.z);
                        ground = true;
                    }
                }
                require(axle && ground);
            }
        }
    }
}
