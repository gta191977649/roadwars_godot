#include "game_src/unresolved/sound_options.hpp"
#include <cmath>

namespace roadwars {
float sound_volume_for_step(int step) { return static_cast<float>(step) * 5.f * .01f; }
Result validate_sound_options(const SoundOptions& s) {
    for (const float gain : {s.music, s.effects, s.race_music})
        if (!std::isfinite(gain) || gain < 0 || gain > 1)
            return Result::invalid_argument;
    return s.channels >= 8 && s.channels <= 32 && (s.channels - 8) % 4 == 0
               ? Result::ok
               : Result::invalid_argument;
}
Result load_sound_options(ResourceStore& resources, SoundOptions& output) {
    Configuration c;
    if (c.load(resources, "Ini/RoadWars.ini") != Result::ok)
        return Result::unavailable;
    SoundOptions s;
    if (c.number("ROADWARS", "MUSIC", s.music) != Result::ok ||
        c.number("ROADWARS", "SFX", s.effects) != Result::ok ||
        c.number("ROADWARS", "INGAMEMUSIC", s.race_music) != Result::ok)
        return Result::invalid_argument;
    float value{};
    if (c.number("ROADWARS", "PLAYMUSICFROMCD", value) == Result::ok)
        s.cd_music = value != 0;
    if (c.number("ROADWARS", "USE3DSOUNDHARDWARE", value) == Result::ok)
        s.hardware_3d = value != 0;
    if (c.number("ROADWARS", "SOUNDCHANNELS", value) == Result::ok) {
        if (!std::isfinite(value) || value != std::trunc(value) || value < 8 || value > 32)
            return Result::invalid_argument;
        s.channels = static_cast<int>(value);
    }
    if (validate_sound_options(s) != Result::ok)
        return Result::invalid_argument;
    output = s;
    return Result::ok;
}
Result override_sound_options(ResourceStore& resources, const SoundOptions& s) {
    if (validate_sound_options(s) != Result::ok)
        return Result::invalid_argument;
    for (const auto& [key, value] :
         {std::pair{"MUSIC", std::to_string(s.music)}, std::pair{"SFX", std::to_string(s.effects)},
          std::pair{"INGAMEMUSIC", std::to_string(s.race_music)},
          std::pair{"PLAYMUSICFROMCD", std::string(s.cd_music ? "1" : "0")},
          std::pair{"USE3DSOUNDHARDWARE", std::string(s.hardware_3d ? "1" : "0")},
          std::pair{"SOUNDCHANNELS", std::to_string(s.channels)}})
        if (resources.set_configuration_override("Ini/RoadWars.ini", "ROADWARS", key, value) !=
            Result::ok)
            return Result::invalid_argument;
    return Result::ok;
}
} // namespace roadwars
