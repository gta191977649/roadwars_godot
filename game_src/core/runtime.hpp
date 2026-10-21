#pragma once
#include "game_src/interfaces/host.hpp"

namespace roadwars {
// Infrastructure smoke fixture; this is not recovered game behavior.
class Runtime {
  public:
    explicit Runtime(Renderer& renderer) : renderer_(renderer) {}
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Result start_smoke_scene();
    Result update(double delta_seconds, const InputState& input);
    void stop();

  private:
    Renderer& renderer_;
    MeshHandle mesh_{};
    InstanceHandle instance_{};
};
} // namespace roadwars
