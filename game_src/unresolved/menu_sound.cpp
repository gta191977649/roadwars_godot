#include "game_src/unresolved/menu_frontend.hpp"
#include <algorithm>

namespace roadwars {
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00476741
Result MenuFrontend::commit_sound_options(std::string& error) {
    SoundOptions s;
    if (custom_.rows.size() != 6 || load_sound_options(resources_, s) != Result::ok)
        return Result::invalid_argument;
    s.cd_music = custom_.rows[0].selected == 0;
    s.music = sound_volume_for_step(custom_.rows[1].selected);
    s.effects = sound_volume_for_step(custom_.rows[2].selected);
    s.race_music = sound_volume_for_step(custom_.rows[3].selected);
    if (validate_sound_options(s) != Result::ok)
        return Result::invalid_argument;
    if (sound_host_ && sound_host_->save_sound_options(s, error) != Result::ok) {
        custom_.message = error;
        return Result::unavailable;
    }
    if (override_sound_options(resources_, s) != Result::ok)
        return Result::invalid_argument;
    sfx_gain_ = s.effects;
    music_gain_ = s.music;
    audio_restart_requested_ = true;
    custom_.message.clear();
    return Result::ok;
}
float MenuFrontend::music_volume() const {
    if (screen_ == "sound" && custom_.rows.size() == 6)
        return sound_volume_for_step(custom_.rows[1].selected);
    return music_gain_;
}
void MenuFrontend::configure_sound_rows() {
    if (custom_.rows.size() != 6)
        return;
    if (custom_.rows[0].selected == 0)
        custom_.message = "CD AUDIO UNAVAILABLE - USING GAME MUSIC";
    // QMDX hardware mixing and voice allocator are unavailable in the Godot backend.
    // Native spatial playback is independent of that legacy hardware preference.
    for (const int i : {4, 5}) {
        custom_.rows[i].available = false;
        custom_.rows[i].values = {"UNAVAILABLE"};
        custom_.rows[i].selected = 0;
    }
}
} // namespace roadwars
