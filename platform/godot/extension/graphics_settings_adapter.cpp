#include "platform/godot/extension/graphics_settings_adapter.hpp"
#include "game_src/core/graphics_settings.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include "platform/godot/rendering/server_renderer.hpp"
#include <algorithm>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <limits>

namespace roadwars {
namespace {
const char* driver(RenderingBackend backend) {
    switch (backend) {
    case RenderingBackend::vulkan:
        return "vulkan";
    case RenderingBackend::direct3d12:
        return "d3d12";
    case RenderingBackend::direct3d11:
        return "opengl3_angle";
    case RenderingBackend::metal:
        return "metal";
    default:
        const auto name = godot::OS::get_singleton()->get_name();
        return name == "Windows" ? "opengl3_angle" : name == "macOS" ? "metal" : "vulkan";
    }
}
const char* rendering_method(RenderingBackend backend) {
    return godot::String(driver(backend)) == "opengl3_angle" ? "gl_compatibility" : "forward_plus";
}
const char* driver_key() {
    const auto name = godot::OS::get_singleton()->get_name();
    return name == "Windows" ? "rendering_device/driver.windows"
           : name == "macOS" ? "rendering_device/driver.macos"
                             : "rendering_device/driver.linuxbsd";
}
constexpr const char* kPath = "user://graphics.cfg";
// A single whitelisted file is both the startup override and confirmed preferences.
void encode(godot::Ref<godot::ConfigFile>& config, const GraphicsSettings& s) {
#define SAVE(field) config->set_value("graphics", #field, s.field)
#define SAVE_ENUM(field) config->set_value("graphics", #field, static_cast<int>(s.field))
    SAVE(monitor);
    SAVE(width);
    SAVE(height);
    SAVE_ENUM(window_mode);
    SAVE(vsync);
    SAVE(fps_limit);
    SAVE_ENUM(backend);
    SAVE_ENUM(aa);
    SAVE(render_scale);
    SAVE_ENUM(texture_quality);
    SAVE_ENUM(texture_filtering);
    SAVE(view_distance);
    SAVE(effects);
    SAVE(skid_marks);
    SAVE(car_detail);
    SAVE(chrome_mapping);
#undef SAVE
#undef SAVE_ENUM
    config->set_value("graphics", "version", 1);
    if (godot::String(driver(s.backend)) != "opengl3_angle")
        config->set_value("rendering", driver_key(), driver(s.backend));
    config->set_value("rendering", "renderer/rendering_method", rendering_method(s.backend));
    if (godot::OS::get_singleton()->get_name() == "Windows") {
        config->set_value("rendering", "renderer/rendering_method.windows", rendering_method(s.backend));
        config->set_value("rendering", "gl_compatibility/driver.windows", "opengl3_angle");
    }
}
GraphicsSettings decode(const godot::Ref<godot::ConfigFile>& config, GraphicsSettings s) {
#define READ(field) s.field = config->get_value("graphics", #field, s.field)
#define READ_ENUM(field, type)                                                                     \
    s.field = static_cast<type>(                                                                   \
        static_cast<int>(config->get_value("graphics", #field, static_cast<int>(s.field))))
    READ(monitor);
    READ(width);
    READ(height);
    READ_ENUM(window_mode, WindowMode);
    READ(vsync);
    READ(fps_limit);
    READ_ENUM(backend, RenderingBackend);
    READ_ENUM(aa, AntiAliasing);
    READ(render_scale);
    READ_ENUM(texture_quality, TextureQuality);
    READ_ENUM(texture_filtering, TextureFiltering);
    READ(view_distance);
    READ(effects);
    READ(skid_marks);
    READ(car_detail);
    READ(chrome_mapping);
#undef READ
#undef READ_ENUM
    return s;
}
bool valid_config_types(const godot::Ref<godot::ConfigFile>& config, godot::String& error) {
    godot::Ref<godot::ConfigFile> schema;
    schema.instantiate();
    encode(schema, GraphicsSettings{});
    for (const auto key : schema->get_section_keys("graphics")) {
        if (!config->has_section_key("graphics", key))
            continue;
        const auto value = config->get_value("graphics", key);
        const auto expected = schema->get_value("graphics", key).get_type();
        const auto type = value.get_type();
        if (type != expected &&
            !(expected == godot::Variant::FLOAT && type == godot::Variant::INT)) {
            error = "Invalid graphics setting type: " + key;
            return false;
        }
        if (type == godot::Variant::INT) {
            const auto integer = static_cast<std::int64_t>(value);
            if (integer < std::numeric_limits<int>::min() ||
                integer > std::numeric_limits<int>::max()) {
                error = "Graphics integer out of range: " + key;
                return false;
            }
        }
    }
    return true;
}
} // namespace
GraphicsSettingsAdapter::GraphicsSettingsAdapter(std::function<ServerRenderer*()> renderer,
                                                 godot::Window* window)
    : renderer_(std::move(renderer)), window_(window) {
    auto* display = godot::DisplayServer::get_singleton();
    for (int i = 0; i < display->get_screen_count(); ++i) {
        const auto size = display->screen_get_size(i);
        capabilities_.monitors.push_back({"DISPLAY " + std::to_string(i + 1), size.x, size.y});
    }
    if (capabilities_.monitors.empty())
        capabilities_.monitors.push_back({"HEADLESS", 960, 540});
    capabilities_.anti_aliasing = {AntiAliasing::off, AntiAliasing::fxaa, AntiAliasing::msaa_2x,
                                   AntiAliasing::msaa_4x, AntiAliasing::msaa_8x};
    capabilities_.texture_quality = capabilities_.texture_filtering = true;
    capabilities_.car_detail = true;
    godot::Array output;
    auto* os = godot::OS::get_singleton();
    godot::PackedStringArray args;
    args.append("--help");
    if (display->get_name() != "headless" &&
        os->execute(os->get_executable_path(), args, output, true, false) == 0) {
        godot::String help;
        for (int i = 0; i < output.size(); ++i)
            help += godot::String(output[i]);
        const auto name = os->get_name();
        for (auto backend :
             {RenderingBackend::direct3d11, RenderingBackend::vulkan,
              RenderingBackend::direct3d12, RenderingBackend::metal}) {
            if (((backend == RenderingBackend::direct3d12 || backend == RenderingBackend::direct3d11) && name != "Windows") ||
                (backend == RenderingBackend::metal && name != "macOS"))
                continue;
            if (help.contains(godot::String("\"") + driver(backend) + "\""))
                capabilities_.backends.push_back(backend);
        }
    }
    const auto active = active_backend();
    if (active != RenderingBackend::automatic &&
        std::find(capabilities_.backends.begin(), capabilities_.backends.end(), active) ==
            capabilities_.backends.end())
        capabilities_.backends.push_back(active);
}
void GraphicsSettingsAdapter::initialize_original_defaults(ResourceStore& resources) {
    if (defaults_initialized_)
        return;
    Configuration original;
    if (original.load(resources, "Ini/RoadWars.ini") != Result::ok)
        return;
    original.number("ROADWARS", "VIEWDISTANCESCALAR", defaults_.view_distance);
    original.number("ROADWARS", "SPECIAL_EFFECTS_SCALAR", defaults_.effects);
    original.number("ROADWARS", "SKIDMARK_SCALAR", defaults_.skid_marks);
    float detail{};
    if (original.number("ROADWARS", "CAR_DETAIL", detail) == Result::ok && detail >= 0 &&
        detail <= 2 && detail == static_cast<int>(detail))
        defaults_.car_detail = static_cast<int>(detail);
    defaults_initialized_ = true;
    if (!godot::FileAccess::file_exists(kPath)) {
        settings_.view_distance = defaults_.view_distance;
        settings_.effects = defaults_.effects;
        settings_.skid_marks = defaults_.skid_marks;
        settings_.car_detail = defaults_.car_detail;
    }
}
GraphicsCapabilities GraphicsSettingsAdapter::capabilities() const { return capabilities_; }
RenderingBackend GraphicsSettingsAdapter::active_backend() const {
    const auto actual =
        godot::RenderingServer::get_singleton()->get_current_rendering_driver_name();
    if (actual == "vulkan")
        return RenderingBackend::vulkan;
    if (actual == "d3d12")
        return RenderingBackend::direct3d12;
    if (actual == "opengl3_angle")
        return RenderingBackend::direct3d11;
    if (actual == "metal")
        return RenderingBackend::metal;
    return RenderingBackend::automatic;
}
bool GraphicsSettingsAdapter::restart_required() const {
    return has_saved_backend_ && godot::String(driver(saved_backend_)) != driver(active_backend());
}
GraphicsSettings GraphicsSettingsAdapter::current_settings() const {
    auto result = settings_;
    if (window_ && godot::DisplayServer::get_singleton()->get_name() != "headless" &&
        result.window_mode == WindowMode::windowed && window_->get_mode() == godot::Window::MODE_WINDOWED) {
        const auto size = window_->get_size();
        if (size.x >= 320 && size.y >= 200) {
            result.width = size.x;
            result.height = size.y;
        }
    }
    return result;
}
void GraphicsSettingsAdapter::load() {
    godot::Ref<godot::ConfigFile> config;
    config.instantiate();
    for (const auto argument : godot::OS::get_singleton()->get_cmdline_user_args()) {
        if (argument == "--reset-graphics") {
            godot::DirAccess::remove_absolute(kPath);
            godot::DirAccess::remove_absolute("user://graphics.cfg.tmp");
            return;
        }
    }
    const auto loaded = config->load(kPath);
    if (loaded != godot::OK) {
        if (godot::FileAccess::file_exists(kPath))
            error_ = "Cannot parse graphics settings";
        return;
    }
    if (!valid_config_types(config, error_))
        return;
    if (static_cast<int>(config->get_value("graphics", "version", 0)) != 1) {
        error_ = "Unsupported graphics settings version";
        return;
    }
    const auto candidate = decode(config, defaults_);
    // A removed monitor should not prevent loading all other saved preferences.
    auto normalized = candidate;
    if (normalized.monitor < 0 ||
        normalized.monitor >= static_cast<int>(capabilities_.monitors.size()))
        normalized.monitor = 0;
    if (validate_graphics_settings(normalized, capabilities_, defaults_) != Result::ok) {
        error_ = "Invalid or unavailable graphics settings";
        return;
    }
    if (apply_live(normalized) != Result::ok)
        error_ = "Cannot apply saved graphics settings";
    else {
        saved_backend_ = normalized.backend;
        has_saved_backend_ = true;
    }
}
Result GraphicsSettingsAdapter::apply_live(const GraphicsSettings& settings) {
    error_ = "";
    if (validate_graphics_settings(settings, capabilities_, settings_) != Result::ok) {
        error_ = "Invalid or unavailable graphics settings";
        return Result::invalid_argument;
    }
    // Reject renderer failures before making any user-visible window changes.
    if (auto* renderer = renderer_()) {
        const auto applied = renderer->apply_graphics_quality(settings);
        if (applied != Result::ok) {
            error_ = "Cannot apply renderer quality settings";
            return applied;
        }
        renderer->set_chrome_mapping_enabled(settings.chrome_mapping);
    }
    auto* display = godot::DisplayServer::get_singleton();
    if (display->get_name() != "headless" && graphics_display_changed(current_settings(), settings)) {
        // Always leave exclusive/fullscreen before moving to a different screen.
        window_->set_mode(godot::Window::MODE_WINDOWED);
        window_->set_flag(godot::Window::FLAG_BORDERLESS, false);
        window_->set_current_screen(settings.monitor);
        if (settings.window_mode == WindowMode::windowed) {
            window_->set_size({settings.width, settings.height});
            const auto rect = display->screen_get_usable_rect(settings.monitor);
            window_->set_position(
                rect.position + (rect.size - godot::Vector2i(settings.width, settings.height)) / 2);
        } else if (settings.window_mode == WindowMode::borderless) {
            // Godot's non-exclusive fullscreen mode is desktop-sized borderless.
            // Resizing a borderless window to desktop size can enter an OS-managed
            // exclusive state that cannot be reverted through the cached window mode.
            window_->set_mode(godot::Window::MODE_FULLSCREEN);
        } else {
            window_->set_mode(godot::Window::MODE_EXCLUSIVE_FULLSCREEN);
        }
    }
    if (display->get_name() != "headless")
        display->window_set_vsync_mode(settings.vsync ? godot::DisplayServer::VSYNC_ENABLED
                                                      : godot::DisplayServer::VSYNC_DISABLED);
    godot::Engine::get_singleton()->set_max_fps(settings.fps_limit);
    settings_ = settings;
    return Result::ok;
}
void GraphicsSettingsAdapter::apply_to_new_renderer() {
    if (auto* renderer = renderer_()) {
        renderer->apply_graphics_quality(settings_);
        renderer->set_chrome_mapping_enabled(settings_.chrome_mapping);
    }
}
bool GraphicsSettingsAdapter::probe(RenderingBackend backend) {
    const auto target = godot::String(driver(backend));
    if (target == driver(active_backend()))
        return true;
    auto* os = godot::OS::get_singleton();
    auto* project = godot::ProjectSettings::get_singleton();
    const auto output = project->globalize_path(
        "user://graphics_probe_" + godot::String::num_int64(os->get_process_id()) + ".txt");
    godot::DirAccess::remove_absolute(output);
    godot::PackedStringArray args;
    for (const auto* argument : {"--path"})
        args.append(argument);
    args.append(project->globalize_path("res://"));
    args.append("--rendering-method");
    args.append(rendering_method(backend));
    args.append("--rendering-driver");
    args.append(target);
    args.append("--position");
    args.append("-10000,-10000");
    args.append("--resolution");
    args.append("64x64");
    args.append("--script");
    args.append("res://graphics_backend_probe.gd");
    args.append("--");
    args.append("--graphics-probe-output=" + output);
    const auto pid = os->create_process(os->get_executable_path(), args, false);
    if (pid <= 0) {
        error_ = "Cannot start graphics backend probe";
        return false;
    }
    const auto deadline = godot::Time::get_singleton()->get_ticks_msec() + 10000;
    while (os->is_process_running(pid) && godot::Time::get_singleton()->get_ticks_msec() < deadline)
        os->delay_usec(10000);
    if (os->is_process_running(pid))
        os->kill(pid);
    const auto result = godot::FileAccess::file_exists(output)
                            ? godot::FileAccess::get_file_as_string(output).strip_edges()
                            : godot::String();
    godot::DirAccess::remove_absolute(output);
    if (result != target) {
        error_ = "Backend probe failed or selected a different driver: " + target;
        return false;
    }
    return true;
}
Result GraphicsSettingsAdapter::persist(const GraphicsSettings& settings) {
    if (validate_graphics_settings(settings, capabilities_, settings_) != Result::ok) {
        error_ = "Cannot save invalid or unavailable graphics settings";
        return Result::invalid_argument;
    }
    if (!probe(settings.backend))
        return Result::unavailable;
    godot::Ref<godot::ConfigFile> config;
    config.instantiate();
    encode(config, settings);
    const auto temporary = godot::String(kPath) + ".tmp";
    if (godot::FileAccess::file_exists(kPath) &&
        godot::DirAccess::copy_absolute(kPath, "user://graphics.last-good.cfg") != godot::OK) {
        error_ = "Cannot preserve previous graphics settings";
        return Result::unavailable;
    }
    if (config->save(temporary) != godot::OK ||
        godot::DirAccess::rename_absolute(temporary, kPath) != godot::OK) {
        error_ = "Cannot save graphics settings";
        return Result::unavailable;
    }
    settings_ = settings;
    saved_backend_ = settings.backend;
    has_saved_backend_ = true;
    error_ = "";
    return Result::ok;
}
Result GraphicsSettingsAdapter::restart() {
    auto* os = godot::OS::get_singleton();
    auto args = os->get_cmdline_args();
    // Drop session-only driver overrides so the confirmed startup configuration wins.
    godot::PackedStringArray restart_args;
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == "--rendering-driver" || args[i] == "--rendering-method") {
            ++i;
            continue;
        }
        if (args[i].begins_with("--rendering-driver=") || args[i].begins_with("--rendering-method="))
            continue;
        restart_args.append(args[i]);
    }
    if (!os->get_cmdline_user_args().is_empty()) {
        restart_args.append("--");
        restart_args.append_array(os->get_cmdline_user_args());
    }
    os->set_restart_on_exit(true, restart_args);
    return Result::ok;
}
} // namespace roadwars
