#pragma once
#include "game_src/core/runtime.hpp"
#include "game_src/unresolved/game_entry.hpp"
#include "platform/godot/rendering/server_renderer.hpp"
#include <godot_cpp/classes/node.hpp>
#include <memory>

namespace roadwars {
class GameHost : public godot::Node, public LoopHooks {
    GDCLASS(GameHost, godot::Node)
  protected:
    static void _bind_methods();
  public:
    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;
    void restart_smoke_scene();
    int get_live_rids() const;
    Result invoke(LoopStage stage, float delta_seconds, const InputState& input,
                  LoopState& state) override;
  private:
    std::unique_ptr<ServerRenderer> renderer_;
    std::unique_ptr<Runtime> runtime_;
    std::unique_ptr<GameEntry> entry_;
    bool original_loop_{};
};
} // namespace roadwars
