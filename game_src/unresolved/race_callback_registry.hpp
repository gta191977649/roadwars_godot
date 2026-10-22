#pragma once
#include "game_src/interfaces/host.hpp"
#include <functional>
#include <list>
#include <string>
namespace roadwars {
// Portable identity of an original callback function/user-data pair. Domain and
// indices replace native pointers without claiming original C++ class names.
struct RaceCallbackKey {
    enum class Domain {
        vehicle,
        scene,
        impact,
        impact_sprite,
        rear_primary,
        rear_secondary,
        nitrous,
        vehicle_sparks,
        scene_electron,
        weather
    };
    Domain domain{};
    std::size_t owner{}, token{};
    bool operator==(const RaceCallbackKey&) const = default;
};
class RaceCallbackRegistry {
  public:
    RaceCallbackRegistry();
    RaceCallbackRegistry(const RaceCallbackRegistry&) = delete;
    RaceCallbackRegistry& operator=(const RaceCallbackRegistry&) = delete;
    void reset();
    void insert(RaceCallbackKey);
    void remove(RaceCallbackKey);
    Result dispatch(const std::function<Result(RaceCallbackKey)>&, std::string& error);
    std::size_t size() const { return entries_.size(); }

  private:
    bool search(RaceCallbackKey);
    void next();
    std::list<RaceCallbackKey> entries_;
    std::list<RaceCallbackKey>::iterator cursor_;
};
} // namespace roadwars
