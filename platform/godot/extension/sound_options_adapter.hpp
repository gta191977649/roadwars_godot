#pragma once
#include "game_src/unresolved/sound_options.hpp"

namespace roadwars {
class SoundOptionsAdapter final : public SoundOptionsHost {
  public:
    Result load(ResourceStore&, std::string& error);
    Result save_sound_options(const SoundOptions&, std::string& error) override;
};
} // namespace roadwars
