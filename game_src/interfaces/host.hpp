#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace roadwars {
struct Vec3 { float x{}, y{}, z{}; };
struct Transform {
    std::array<Vec3, 3> basis{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    Vec3 origin{};
};
struct MeshHandle { std::uint64_t value{}; };
struct InstanceHandle { std::uint64_t value{}; };
struct InputState { float steering{}, throttle{}, brake{}; bool quit_requested{}; };
enum class Result { ok, invalid_argument, unavailable, not_found };
class ResourceReader {
  public:
    virtual ~ResourceReader() = default;
    virtual Result read(std::string_view path, std::vector<std::byte>& output) = 0;
};
class Logger {
  public:
    virtual ~Logger() = default;
    virtual void log(std::string_view message) = 0;
};
class Renderer {
  public:
    virtual ~Renderer() = default;
    virtual MeshHandle upload_mesh(std::span<const Vec3> vertices,
                                   std::span<const std::uint32_t> indices) = 0;
    virtual InstanceHandle create_instance(MeshHandle mesh) = 0;
    virtual Result update_instance(InstanceHandle instance, const Transform& transform) = 0;
    virtual void destroy_instance(InstanceHandle instance) = 0;
    virtual void destroy_mesh(MeshHandle mesh) = 0;
    virtual void update_camera(const Transform& transform, float vertical_fov_degrees) = 0;
};
} // namespace roadwars
