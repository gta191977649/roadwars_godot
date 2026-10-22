#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <optional>

namespace roadwars {
enum class WeaponSlot { front, side, rear };
// These are configured display names, not claimed original C++ class names.
struct WeaponDescriptor {
    std::string_view name;
    WeaponSlot slot;
    std::uint32_t constructor_address{}, initialization_address{}, vtable_address{};
};
std::span<const WeaponDescriptor> weapon_descriptors();
const WeaponDescriptor* find_weapon_descriptor(std::string_view name);
bool weapon_is_guided(std::string_view name);
std::string_view weapon_slot_name(WeaponSlot);
struct WeaponParameters {
    float hitpoints{}, ammunition_count{}, top_speed{}, acceleration{}, impact_multiplier{};
    float rotational_speed{}, ammunition_mass_kg{}, tracking_seconds{};
    std::int32_t default_ammunition{};
};
Result load_weapon_parameters(ResourceStore&, const WeaponDescriptor&, WeaponParameters&,
                              std::string& error);
struct WeaponFireRequest {
    Vec3 origin{};
    float heading{}, pitch{}, speed{};
};
// Portable mount state maps 004cfc3d; it borrows weapon readiness from its real implementation.
// The caller must execute each emitted request, rather than treating emission as a hit.
class WeaponMountRuntime {
  public:
    Result initialize(WeaponSlot slot, std::int32_t ammunition);
    bool request(float speed, bool weapon_ready);
    Result advance(float dt, bool weapon_idle, float heading, float pitch, Vec3 origin,
                   std::optional<WeaponFireRequest>& fired);
    void cancel();
    bool pending() const { return pending_; }
    bool visible() const { return visible_; }
    float deployment_roll() const { return deployment_roll_; }
    std::int32_t ammunition() const { return ammunition_; }
    WeaponSlot slot() const { return slot_; }

  private:
    WeaponSlot slot_{};
    std::int32_t ammunition_{};
    float deployment_roll_{}, launch_speed_{};
    bool pending_{}, deployed_{}, visible_{}, initialized_{};
};
} // namespace roadwars
