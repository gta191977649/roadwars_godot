#pragma once
#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <map>

namespace roadwars {
// Bounded fallback used when an original MSH cache is absent.
Result decode_frontend_d3d(std::span<const std::byte>, MeshObject&, std::string& error);

// New portable presentation owner. Business/race state is intentionally outside this class.
class FrontendScene {
  public:
    FrontendScene(ResourceStore& resources, Renderer& renderer)
        : resources_(resources), renderer_(renderer) {}
    ~FrontendScene();
    FrontendScene(const FrontendScene&) = delete;
    FrontendScene& operator=(const FrontendScene&) = delete;
    Result load(std::string_view id, std::string_view character, int variant, std::string& error);
    Result advance(double elapsed_seconds);
    // Empty labels select the first configured item/quality. No purchase or save is performed.
    Result select_store_product(std::string_view item, std::string_view quality,
                                std::string& error);
    void stop();
    bool loaded() const { return loaded_; }
    Vec3 camera_position() const { return eye_; }
    std::size_t camera_index() const { return camera_index_; }
    double elapsed_seconds() const { return elapsed_; }

  private:
    struct Draw {
        InstanceHandle instance;
        Transform local;
        int vehicle{-1}, turntable{-1}, sky{-1};
        bool product{};
    };
    struct CameraPose {
        Vec3 eye, target;
        float seconds{};
    };
    Result read_mesh(const std::string& path, MeshObject&, std::string& error);
    MaterialHandle material(const MeshPart&, const std::string& base, std::uint32_t flags);
    void upload(const MeshObject&, const std::string& base, Vec3 parent, int vehicle = -1,
                int turntable = -1, int sky = -1);
    void upload_vehicle(std::string_view name, Vec3 anchor, int index);
    void upload_prizes(std::size_t rank, int money, int points);
    void update_draws();
    void update_camera();
    void step(float dt);
    ResourceStore& resources_;
    Renderer& renderer_;
    bool loaded_{}, victory_{}, store_{}, victory_finished_{};
    double elapsed_{}, debt_{};
    Vec3 eye_{}, velocity_{}, target_{}, stand_{};
    Vec3 product_anchor_{}, product_angles_{}, product_speeds_{};
    std::string character_;
    int store_variant_{};
    std::size_t product_draw_begin_{}, product_mesh_begin_{};
    std::size_t camera_index_{};
    float camera_seconds_{}, rotation_speed_{};
    float camera_heading_{}, camera_pitch_{}, heading_speed_{};
    std::vector<CameraPose> cameras_;
    std::vector<Vec3> anchors_;
    std::vector<float> vehicle_angles_, vehicle_heights_, sky_angles_;
    std::vector<Draw> draws_;
    std::vector<MeshHandle> meshes_;
    std::vector<TextureHandle> textures_;
    std::map<std::string, MaterialHandle> materials_;
};
} // namespace roadwars
