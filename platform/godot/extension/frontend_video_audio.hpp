#pragma once
#include "game_src/unresolved/engine_audio.hpp"
#include "game_src/unresolved/frontend_video.hpp"

namespace roadwars {
// Host codec conversion only. Portable code retains encoded data and playback timing.
Result decode_frontend_video_audio(const FrontendVideoAudio& source, WavePcm& output,
                                   std::string& error);
} // namespace roadwars
