#pragma once
#include "game_src/interfaces/graphics_settings.hpp"
#include <functional>
#include <godot_cpp/variant/string.hpp>

namespace roadwars {
class ServerRenderer;
class ResourceStore;
class GraphicsSettingsAdapter final : public GraphicsSettingsHost {
  public:
    explicit GraphicsSettingsAdapter(std::function<ServerRenderer*()> renderer);
    GraphicsCapabilities capabilities() const override;
    GraphicsSettings current_settings() const override { return settings_; }
    GraphicsSettings default_settings() const override { return defaults_; }
    Result apply_live(const GraphicsSettings&) override;
    Result persist(const GraphicsSettings&) override;
    RenderingBackend active_backend() const override;
    bool restart_required() const override;
    Result restart() override;
    void load();
    void initialize_original_defaults(ResourceStore& resources);
    const godot::String& error() const { return error_; }
    void apply_to_new_renderer();
  private:
    bool probe(RenderingBackend);
    std::function<ServerRenderer*()> renderer_;
    GraphicsSettings settings_, defaults_;
    RenderingBackend saved_backend_{RenderingBackend::automatic};
    bool has_saved_backend_{};
    GraphicsCapabilities capabilities_;
    godot::String error_;
    bool defaults_initialized_{};
};
} // namespace roadwars
