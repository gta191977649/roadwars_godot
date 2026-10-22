#pragma once
#include "game_src/unresolved/weapon_runtime.hpp"

namespace roadwars {
struct ContactWeaponPose {
    float local_z{};
    bool extending{}, active{};
};
bool contact_weapon_front_hit(Vec3 attacker_forward, Vec3 attacker_position, Vec3 contact_point);
// Portable fields of00407ec0/00455bc0; original C++ class names remain unknown.
class ContactWeaponRuntime {
  public:
    Result initialize(std::string_view weapon_name, float weapon_model_length, std::string& error);
    bool begin(const WeaponFireRequest&);
    Result advance(float dt);
    void cancel() { active_ = false; }
    bool ready() const { return initialized_ && !active_; }
    bool active() const { return active_; }
    float front_extension() const { return weapon_length_; }
    ContactWeaponPose pose() const { return {local_z_, extending_, active_}; }
    // Apply only to a force already produced by the recovered vehicle-pair collision path.
    // This function does not detect contacts or synthesize impact forces/damage.
    Result modify_contact_force(Vec3 input, Vec3& output) const;

  private:
    float weapon_length_{}, local_z_{};
    bool hydraulic_{}, extending_{}, active_{}, callback_running_{}, initialized_{};
};
} // namespace roadwars
