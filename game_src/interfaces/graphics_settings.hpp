#pragma once

#include "game_src/interfaces/host.hpp"

namespace roadwars {
// Modern host policy; these types do not assert recovered original class ownership.
enum class WindowMode { windowed, borderless, fullscreen };
enum class RenderingBackend { automatic, vulkan, direct3d12, metal, direct3d11 };
enum class AntiAliasing { off, fxaa, msaa_2x, msaa_4x, msaa_8x };
enum class TextureQuality { high, medium, low };
enum class TextureFiltering {
    linear,
    anisotropic_2x,
    anisotropic_4x,
    anisotropic_8x,
    anisotropic_16x
};

struct GraphicsSettings {
    int monitor{}, width{960}, height{540};
    WindowMode window_mode{WindowMode::windowed};
    bool vsync{true};
    int fps_limit{}; // Zero means unlimited, without changing simulation frequency.
    RenderingBackend backend{RenderingBackend::automatic};
    AntiAliasing aa{AntiAliasing::off};
    float render_scale{1};
    TextureQuality texture_quality{TextureQuality::high};
    TextureFiltering texture_filtering{TextureFiltering::linear};
    float view_distance{1}, effects{1}, skid_marks{1};
    int car_detail{2}; // Low, medium, high; actual LOD policy belongs to recovered code.
    bool chrome_mapping{true};
    bool operator==(const GraphicsSettings&) const = default;
};

struct GraphicsMonitor {
    std::string name;
    int width{}, height{};
};
struct GraphicsCapabilities {
    std::vector<GraphicsMonitor> monitors;
    std::vector<RenderingBackend> backends{RenderingBackend::automatic};
    std::vector<AntiAliasing> anti_aliasing{AntiAliasing::off};
    bool texture_quality{}, texture_filtering{}, car_detail{};
};

class GraphicsSettingsHost {
  public:
    virtual ~GraphicsSettingsHost() = default;
    virtual GraphicsCapabilities capabilities() const = 0;
    virtual GraphicsSettings current_settings() const = 0;
    virtual GraphicsSettings default_settings() const { return {}; }
    // Must leave the previous live state intact on failure. Never switch backend here.
    virtual Result apply_live(const GraphicsSettings& settings) = 0;
    // Atomically saves the entire confirmed candidate, including startup-only backend.
    virtual Result persist(const GraphicsSettings& settings) = 0;
    virtual RenderingBackend active_backend() const { return RenderingBackend::automatic; }
    virtual RenderingBackend saved_backend() const { return current_settings().backend; }
    virtual std::string last_error() const { return {}; }
    virtual bool restart_required() const { return false; }
    virtual Result restart() { return Result::unavailable; }
};
} // namespace roadwars
