#pragma once
#include "game_src/interfaces/host.hpp"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <unordered_map>

namespace roadwars {
class ServerRenderer final : public Renderer {
  public:
    explicit ServerRenderer(godot::RID viewport);
    ~ServerRenderer() override;
    ServerRenderer(const ServerRenderer&) = delete;
    ServerRenderer& operator=(const ServerRenderer&) = delete;
    MeshHandle upload_mesh(std::span<const Vec3>, std::span<const std::uint32_t>) override;
    InstanceHandle create_instance(MeshHandle) override;
    Result update_instance(InstanceHandle, const Transform&) override;
    void destroy_instance(InstanceHandle) override;
    void destroy_mesh(MeshHandle) override;
    void update_camera(const Transform&, float vertical_fov_degrees) override;
    std::size_t live_rids() const;

  private:
    godot::RenderingServer* server_{};
    godot::RID viewport_, scenario_, camera_, shader_, material_;
    std::uint64_t next_handle_{1};
    std::unordered_map<std::uint64_t, godot::RID> meshes_, instances_;
};
} // namespace roadwars
