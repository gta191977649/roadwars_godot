#pragma once
#include "game_src/interfaces/race_debug.hpp"
#include "game_src/unresolved/race_ai.hpp"
#include "game_src/unresolved/race_frontend.hpp"
#include "game_src/unresolved/race_start.hpp"
#include <memory>

namespace roadwars {
struct RaceOptions {
    std::string track{"Dark Forest"}, car{"Animal"};
    int laps{3}, ai_count{5};
    std::uint32_t seed{1}; // Diagnostic seed, not an original startup default claim.
};
enum class RacePhase { countdown, racing, finished };
struct RaceParticipant {
    std::string character;
    std::unique_ptr<VehicleDynamics> owned_vehicle;
    VehicleDynamics* vehicle{};
    RaceAi ai;
    OriginalChaseCamera chase_camera;
    CameraFrame camera;
    RaceRouteProgress progress;
    std::array<float, 10> lap_times{};
    double finish_time{};
    int rank{};
};
// Portable owner inferred from race+0348/+034c and vehicle+4418 links.
// Original C++ class name and module remain unresolved.
class RaceSession {
  public:
    ~RaceSession();
    Result load(ResourceStore&, const TrackCollision&, const RaceOptions&, VehicleDynamics& player,
                std::string& error);
    Result advance(float delta, const InputState&, std::string& error);
    void set_paused(bool value) { paused_ = value; }
    bool paused() const { return paused_; }
    void set_player_autopilot(bool value) { player_autopilot_ = value; }
    Result restart(std::string& error);
    std::span<const RaceParticipant> racers() const { return racers_; }
    const RaceRoute& route() const { return route_; }
    const CameraFrame& camera_for(int ai) const {
        return ai >= 0 && static_cast<std::size_t>(ai) < player_index() ? racers_[ai].camera
                                                                        : start_.camera();
    }
    const RaceOptions& options() const { return options_; }
    std::size_t player_index() const { return racers_.empty() ? 0 : racers_.size() - 1; }
    RacePhase phase() const { return phase_; }
    double time() const { return race_time_; }
    float countdown() const { return countdown_; }
    RaceStart& start_sequence() { return start_; }
    const RaceStart& start_sequence() const { return start_; }
    std::uint64_t ticks() const { return ticks_; }
    FrontendFrame frontend_frame(std::vector<FrontendRacer>& storage) const;
    RaceDebugSnapshot debug_snapshot() const;
    std::vector<std::size_t> ranking() const { return ranking_; }

  private:
    TrackSpawn rescue(std::size_t) const;
    std::uint32_t random();
    void update_ranking();
    RaceStart start_;
    RaceOptions options_;
    RaceRoute route_;
    std::vector<RaceParticipant> racers_;
    std::vector<TrackSpawn> spawns_;
    std::vector<std::size_t> ranking_, finished_;
    std::vector<RaceDebugLine> collision_lines_;
    const TrackCollision* collision_{};
    float initial_countdown_{}, countdown_{};
    double race_time_{};
    std::uint64_t ticks_{};
    std::uint32_t random_state_{};
    std::size_t scanner_{};
    bool paused_{}, player_autopilot_{};
    RacePhase phase_{RacePhase::countdown};
};
} // namespace roadwars
