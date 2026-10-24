#include "platform/godot/extension/sound_options_adapter.hpp"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

namespace roadwars {
namespace {
constexpr const char* kPath = "user://sound.cfg";
}
Result SoundOptionsAdapter::load(ResourceStore& resources, std::string& error) {
    if (!godot::FileAccess::file_exists(kPath))
        return Result::ok;
    godot::Ref<godot::ConfigFile> file;
    file.instantiate();
    SoundOptions s;
    if (file->load(kPath) != godot::OK || load_sound_options(resources, s) != Result::ok) {
        error = "Cannot read saved sound options";
        return Result::unavailable;
    }
    for (const auto* key : {"MUSIC", "SFX", "INGAMEMUSIC"}) {
        const auto v = file->get_value("sound", key, -1.0);
        if (v.get_type() != godot::Variant::FLOAT && v.get_type() != godot::Variant::INT) {
            error = "Invalid saved sound volume";
            return Result::invalid_argument;
        }
        if (std::string_view(key) == "MUSIC")
            s.music = static_cast<double>(v);
        else if (std::string_view(key) == "SFX")
            s.effects = static_cast<double>(v);
        else
            s.race_music = static_cast<double>(v);
    }
    const auto cd = file->get_value("sound", "PLAYMUSICFROMCD", "");
    const auto hardware = file->get_value("sound", "USE3DSOUNDHARDWARE", "");
    const auto channels = file->get_value("sound", "SOUNDCHANNELS", -1);
    const auto version = file->get_value("sound", "version", 0);
    if (cd.get_type() != godot::Variant::BOOL || hardware.get_type() != godot::Variant::BOOL ||
        channels.get_type() != godot::Variant::INT ||
        version.get_type() != godot::Variant::INT || static_cast<std::int64_t>(version) != 1) {
        error = "Invalid saved sound options";
        return Result::invalid_argument;
    }
    s.cd_music = cd;
    s.hardware_3d = hardware;
    const std::int64_t count = channels;
    if (count < 8 || count > 32) {
        error = "Invalid saved sound channel count";
        return Result::invalid_argument;
    }
    s.channels = static_cast<int>(count);
    const auto result = override_sound_options(resources, s);
    if (result != Result::ok)
        error = "Invalid saved sound options";
    return result;
}
Result SoundOptionsAdapter::save_sound_options(const SoundOptions& s, std::string& error) {
    if (validate_sound_options(s) != Result::ok)
        return Result::invalid_argument;
    godot::Ref<godot::ConfigFile> file;
    file.instantiate();
    file->set_value("sound", "version", 1);
    file->set_value("sound", "MUSIC", s.music);
    file->set_value("sound", "SFX", s.effects);
    file->set_value("sound", "INGAMEMUSIC", s.race_music);
    file->set_value("sound", "PLAYMUSICFROMCD", s.cd_music);
    file->set_value("sound", "USE3DSOUNDHARDWARE", s.hardware_3d);
    file->set_value("sound", "SOUNDCHANNELS", s.channels);
    if (file->save("user://sound.cfg.tmp") != godot::OK ||
        godot::DirAccess::rename_absolute("user://sound.cfg.tmp", kPath) != godot::OK) {
        error = "Cannot save sound options";
        return Result::unavailable;
    }
    error.clear();
    return Result::ok;
}
} // namespace roadwars
