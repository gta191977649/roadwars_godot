#pragma once
#include "game_src/interfaces/graphics_settings.hpp"
#include <functional>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/string.hpp>

namespace roadwars {
class ServerRenderer;
class ResourceStore;
class GraphicsSettingsAdapter final : public GraphicsSettingsHost {
  public:
    explicit GraphicsSettingsAdapter(std::function<ServerRenderer*()> renderer,
                                     godot::Window* window);
    GraphicsCapabilities capabilities() const override;
    GraphicsSettings current_settings() const override;
    GraphicsSettings default_settings() const override { return defaults_; }
    Result apply_live(const GraphicsSettings&) override;
    Result persist(const GraphicsSettings&) override;
    RenderingBackend active_backend() const override;
    RenderingBackend saved_backend() const override { return saved_backend_; }
    std::string last_error() const override { return error_.utf8().get_data(); }
    bool restart_required() const override;
    Result restart() override;
    void load();
    void initialize_original_defaults(ResourceStore& resources);
    const godot::String& error() const { return error_; }
    void apply_to_new_renderer();

  private:
    bool probe(RenderingBackend);
    std::function<ServerRenderer*()> renderer_;
    godot::Window* window_; // Borrowed bootstrap window.
    GraphicsSettings settings_, defaults_;
    RenderingBackend saved_backend_{RenderingBackend::automatic};
    bool has_saved_backend_{};
    GraphicsCapabilities capabilities_;
    godot::String error_;
    bool defaults_initialized_{};
};
} // namespace roadwars
