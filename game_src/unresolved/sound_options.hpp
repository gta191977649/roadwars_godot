#pragma once
#include "game_src/unresolved/resource_store.hpp"

namespace roadwars {
struct SoundOptions {
    bool cd_music{}, hardware_3d{};
    float music{}, effects{}, race_music{};
    int channels{20};
    bool operator==(const SoundOptions&) const = default;
};
Result load_sound_options(ResourceStore&, SoundOptions&);
Result validate_sound_options(const SoundOptions&);
float sound_volume_for_step(int step);
Result override_sound_options(ResourceStore&, const SoundOptions&);
// New host persistence boundary; original files remain read-only.
class SoundOptionsHost {
  public:
    virtual ~SoundOptionsHost() = default;
    virtual Result save_sound_options(const SoundOptions&, std::string& error) = 0;
};
} // namespace roadwars
