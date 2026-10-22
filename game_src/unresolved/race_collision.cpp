#include "game_src/unresolved/race_collision.hpp"
#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
constexpr float kDegreesToRadians = 0.017455555498600006f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float f) { return {a.x * f, a.y * f, a.z * f}; }
float modulus(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
Vec3 normalized(Vec3 a) {
    const float length = modulus(a);
    return length == 0 ? a : scale(a, 1 / length);
}
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float angle_delta(float to, float from) {
    if (to < 0)
        to += 360;
    else if (to >= 360)
        to -= 360;
    if (from < 0)
        from += 360;
    else if (from >= 360)
        from -= 360;
    float result = to - from;
    if (std::abs(result) > 180)
        result = to > from ? -(360 - to + from) : 360 - from + to;
    return result;
}
struct Edge {
    Vec3 from, to;
};
struct Intersection {
    Edge a, b;
    Vec3 point;
    std::size_t a_index{}, b_index{};
};
using Intersections = std::vector<Intersection>;

// 004dd310's XZ segment contract, including axis-aligned collinear contacts.
bool intersect(Edge a, Edge b, Vec3& point) {
    const float x1 = a.from.x, z1 = a.from.z, x2 = a.to.x, z2 = a.to.z;
    const float x3 = b.from.x, z3 = b.from.z, x4 = b.to.x, z4 = b.to.z;
    const float ax = x1 - x2, az = z1 - z2, bx = -(x3 - x4), bz = -(z3 - z4);
    if ((bx == 0 && bz == 0) || (ax == 0 && az == 0))
        return false;
    const auto overlaps = [](float a1, float a2, float b1, float b2) {
        return ((a1 <= b1 && b2 <= a1) || (a1 <= b2 && b1 <= a1) || (a2 <= b1 && b2 <= a2) ||
                (a2 <= b2 && b1 <= a2));
    };
    if (bx == 0 && ax == 0) {
        if (x1 != x3)
            return false;
        if (overlaps(z1, z2, z3, z4)) {
            point = {x1, 0, (z1 + z2 + z3 + z4) / 4};
            return true;
        }
    }
    if (bz == 0 && az == 0) {
        if (z1 != z3)
            return false;
        if (overlaps(x1, x2, x3, x4)) {
            point = {(x1 + x2 + x3 + x4) / 4, 0, z1};
            return true;
        }
    }
    if (bx != 0 && ax != 0 && az / ax == bz / bx)
        return false;
    float ta{}, tb{};
    if (bz == 0 || bx == 0)
        ta = bx == 0 ? (x3 - x1) / ax : (z3 - z1) / az;
    else
        ta = ((z3 - z1) + (x3 - x1) * (-bz / bx)) / (ax * (-bz / bx) + az);
    if (ax == 0 || az == 0)
        tb = az == 0 ? (z3 - z1) / bz : (x3 - x1) / bx;
    else
        tb = ((z3 - z1) + (x3 - x1) * (-az / ax)) / (bx * (-az / ax) + bz);
    if (ta > 0 || ta < -1 || tb > 0 || tb < -1 || !std::isfinite(ta) || !std::isfinite(tb))
        return false;
    point = {ax * ta + x1, 0, az * ta + z1};
    return true;
}
Intersections intersections(const VehicleParameters& ap, const VehicleState& a,
                            const VehicleParameters& bp, const VehicleState& b) {
    const auto ac = vehicle_pair_footprint(ap, a), bc = vehicle_pair_footprint(bp, b);
    Intersections result;
    result.reserve(16);
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) {
            const Edge ae{ac[i], ac[(i + 1) % 4]}, be{bc[j], bc[(j + 1) % 4]};
            Vec3 point;
            if (intersect(ae, be, point))
                result.push_back({ae, be, point, i, j});
        }
    return result;
}
bool sides(Edge a, Edge b, Vec3& normal, float& cosine) {
    normal = normalized(normal);
    cosine = dot(normalized(subtract(a.from, b.from)), normal);
    if (cosine < 0)
        return false;
    cosine = dot(normalized(subtract(a.to, b.from)), normal);
    return cosine >= 0;
}
struct Response {
    Vec3 normal, point;
    bool receiver_b{};
};
Response response(std::span<const Intersection> contacts) {
    Response result;
    float maximum = -999;
    for (const auto& contact : contacts) {
        auto difference = subtract(contact.b.from, contact.b.to);
        Vec3 normal{difference.z, 0, -difference.x};
        float cosine{};
        if (sides(contact.a, contact.b, normal, cosine))
            return {normal, contact.point, false};
        if (maximum < cosine) {
            maximum = cosine;
            result = {normal, contact.point, false};
        }
        difference = subtract(contact.a.from, contact.a.to);
        normal = {difference.z, 0, -difference.x};
        if (sides(contact.b, contact.a, normal, cosine))
            return {normal, contact.point, true};
        if (maximum < cosine) {
            maximum = cosine;
            result = {normal, contact.point, true};
        }
    }
    return result;
}
Vec3 contact_velocity(const VehicleState& state, Vec3 point) {
    if (state.yaw_rate == 0)
        return state.velocity;
    const Vec3 offset = subtract(point, state.position);
    const float distance = modulus(offset);
    const float angle = (state.yaw_rate <= 0 ? -90.0f : 90.0f) * kDegreesToRadians;
    const float sine = static_cast<float>(std::sin(static_cast<double>(angle)));
    const float cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
    const Vec3 rotated{offset.x * cosine + offset.z * sine, offset.y,
                       offset.z * cosine - offset.x * sine};
    return add(state.velocity, scale(scale(rotated, distance), kDegreesToRadians * state.yaw_rate));
}
void impulses(const VehicleParameters& ap, VehicleState& a, const VehicleParameters& bp,
              VehicleState& b, Vec3 force_on_a, Vec3 point) {
    apply_vehicle_impulse(a.velocity, force_on_a, .65f, ap.inverse_mass, false);
    apply_vehicle_yaw_impulse(ap, a, force_on_a, point, .6f, false, -1);
    const Vec3 force_on_b = scale(force_on_a, -1);
    apply_vehicle_impulse(b.velocity, force_on_b, .65f, bp.inverse_mass, false);
    apply_vehicle_yaw_impulse(bp, b, force_on_b, point, .6f, false, -1);
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041d580
std::array<Vec3, 4> vehicle_pair_footprint(const VehicleParameters& p, const VehicleState& state) {
    const float half_length = p.length * .5f, half_width = p.width * .5f;
    std::array<Vec3, 4> corners{{{-half_width, 0, half_length + p.front_extension},
                                 {half_width, 0, half_length + p.front_extension},
                                 {half_width, 0, -half_length},
                                 {-half_width, 0, -half_length}}};
    const float radians = state.heading_degrees * kDegreesToRadians;
    const float sine = static_cast<float>(std::sin(static_cast<double>(radians)));
    const float cosine = static_cast<float>(std::cos(static_cast<double>(radians)));
    for (auto& corner : corners) {
        const float x = corner.x;
        corner.x = cosine * x + sine * corner.z + state.position.x;
        corner.y = 0; // The pair solver flattens both rectangles before intersection.
        corner.z = (cosine * corner.z - x * sine) + state.position.z;
    }
    return corners;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041e5bd
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041d74d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041d92c
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041d9d4
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041e180
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0041de2a
VehiclePairContact solve_vehicle_collision(const VehicleParameters& ap, VehicleState& a,
                                           const VehicleState& previous_a,
                                           const VehicleParameters& bp, VehicleState& b,
                                           const VehicleState& previous_b, float dt,
                                           const std::function<void(Vec3, Vec3, bool)>& damage,
                                           const std::function<void(Vec3, Vec3, bool)>& effect) {
    VehiclePairContact result;
    if (!std::isfinite(dt) || dt <= 0 || ap.mass_kg <= 0 || bp.mass_kg <= 0 || ap.length <= 0 ||
        ap.width <= 0 || bp.length <= 0 || bp.width <= 0 || ap.yaw_inertia <= 0 ||
        bp.yaw_inertia <= 0)
        return result;
    // 0041e3a6: route-index pruning belongs to the race caller.
    if (modulus(subtract(a.position, b.position)) > ap.length + bp.length + 2)
        return result;
    const auto& lower = a.position.y < b.position.y ? a : b;
    const auto& lower_parameters = a.position.y < b.position.y ? ap : bp;
    const float height =
        static_cast<float>(std::abs(std::sin(static_cast<double>(lower.pitch_degrees))) *
                               std::max(lower_parameters.length - lower_parameters.height, 0.0f) +
                           lower_parameters.height);
    if (std::abs(a.position.y - b.position.y) > height)
        return result;
    auto contacts = intersections(ap, a, bp, b);
    if (contacts.empty())
        return result;
    result.hit = true;
    const auto current_a = a, current_b = b;
    const Vec3 displacement_a = subtract(a.position, previous_a.position);
    const Vec3 displacement_b = subtract(b.position, previous_b.position);
    const float turn_a = angle_delta(previous_a.heading_degrees, a.heading_degrees);
    const float turn_b = angle_delta(previous_b.heading_degrees, b.heading_degrees);
    float fraction{};
    bool fallback{};
    auto candidate_a = a, candidate_b = b;
    auto last_contacts = contacts;
    unsigned iteration{};
    for (;;) {
        if (iteration > 1 && contacts.empty())
            break;
        if (contacts.empty())
            fraction -= (1 - fraction) / 1.9500000476837158f;
        else {
            fraction += (1 - fraction) / 1.9500000476837158f;
            last_contacts = contacts;
        }
        if (fraction < 0)
            fraction = 0;
        if (iteration > 19) {
            fallback = true;
            break;
        }
        candidate_a.position = subtract(current_a.position, scale(displacement_a, fraction));
        candidate_b.position = subtract(current_b.position, scale(displacement_b, fraction));
        candidate_a.heading_degrees = current_a.heading_degrees + turn_a * fraction;
        candidate_b.heading_degrees = current_b.heading_degrees + turn_b * fraction;
        contacts = intersections(ap, candidate_a, bp, candidate_b);
        ++iteration;
    }
    result.rollback_iterations = iteration;
    result.fallback = fallback;
    a.position = candidate_a.position;
    b.position = candidate_b.position;
    a.heading_degrees = candidate_a.heading_degrees;
    b.heading_degrees = candidate_b.heading_degrees;
    // 004288e2(...,0) commits the original candidate headings with one wrap.
    const auto wrap = [](float value) {
        if (value < 0)
            return value + 360;
        if (value >= 360)
            return value - 360;
        return value;
    };
    a.heading_degrees = wrap(a.heading_degrees);
    b.heading_degrees = wrap(b.heading_degrees);
    Vec3 force;
    if (fallback) {
        force = scale(displacement_a, -bp.mass_kg);
        result.point = previous_a.position;
        result.normal = normalized(scale(force, -1));
        result.impact_strength = modulus(force);
        if (damage)
            damage(scale(force, -1), result.point, true);
        // Fallback 0041ecc5: first argument is A, so force is received by B.
        impulses(ap, a, bp, b, scale(force, -1), result.point);
    } else {
        // Original records retain edge pointers into the candidate arrays; a clear
        // candidate updates those edges without replacing the last contact points.
        const auto corners_a = vehicle_pair_footprint(ap, candidate_a);
        const auto corners_b = vehicle_pair_footprint(bp, candidate_b);
        for (auto& record : last_contacts) {
            record.a = {corners_a[record.a_index], corners_a[(record.a_index + 1) % 4]};
            record.b = {corners_b[record.b_index], corners_b[(record.b_index + 1) % 4]};
        }
        const auto selected = response(last_contacts);
        const Vec3 av = contact_velocity(a, last_contacts.front().point);
        const Vec3 bv = contact_velocity(b, last_contacts.front().point);
        const Vec3 am =
            scale({av.x * selected.normal.x, av.y * selected.normal.y, av.z * selected.normal.z},
                  ap.mass_kg);
        const Vec3 bm =
            scale({bv.x * selected.normal.x, bv.y * selected.normal.y, bv.z * selected.normal.z},
                  bp.mass_kg);
        result.impact_strength = modulus(subtract(am, bm));
        result.sound_on_b = !selected.receiver_b;
        force = scale(selected.normal, result.impact_strength);
        result.point = selected.point;
        result.normal = selected.receiver_b ? scale(selected.normal, -1) : selected.normal;
        if (damage)
            damage(selected.receiver_b ? scale(force, -1) : force, result.point, false);
        impulses(ap, a, bp, b, selected.receiver_b ? scale(force, -1) : force, result.point);
    }
    if (effect)
        effect(result.normal, result.point, result.sound_on_b);
    result.removed_seconds = fraction * dt;
    a.position = add(a.position, scale(a.velocity, result.removed_seconds));
    b.position = add(b.position, scale(b.velocity, result.removed_seconds));
    a.cached_speed = modulus(a.velocity);
    b.cached_speed = modulus(b.velocity);
    return result;
}

// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004215fd
// Native health then vehicle-owned body/paint mutation, sharing the race CRT stream.
Result apply_vehicle_collision_damage(VehicleDynamics& receiver, Vec3 force, Vec3 point,
                                      bool invulnerable, bool player, int difficulty) {
    auto& health = receiver.equipment();
    if (invulnerable || health.dying_timer != 0)
        return Result::ok;
    const auto& p = receiver.parameters();
    float amount = (1 / p.mass_kg) * .015978554263710976f * modulus(force);
    if (amount < .029999999329447746f)
        return Result::ok;
    if (player) {
        constexpr std::array<float, 4> multipliers{.5f, .699999988079071f, .8500000238418579f, 1};
        amount *= multipliers[static_cast<std::size_t>(std::clamp(difficulty, 0, 3))];
    }
    auto local = subtract(point, receiver.state().position);
    // D3DRMVectorRotate normalizes its result; the following original
    // VectorScale restores the saved modulus, rather than squaring it.
    const auto distance = modulus(local);
    local = normalized(local);
    const float angle = -receiver.state().heading_degrees * kDegreesToRadians;
    const float sine = std::sin(angle), cosine = std::cos(angle);
    local = {distance * (local.x * cosine + local.z * sine), distance * local.y,
             distance * (local.z * cosine - local.x * sine)};
    const float width = p.width, length = p.length;
    std::optional<std::size_t> armor;
    if (-width < local.x && local.x <= 0 && -length * .33f <= local.z && local.z <= length * .33f)
        armor = 3;
    else if (-width < local.x && local.x < width && -length < local.z && local.z < -length * .33f)
        armor = 2;
    else if (-width < local.x && local.x < width && length * .33f < local.z && local.z < length)
        armor = 0;
    // The duplicated negative-X predicate in original004211a0 leaves its
    // positive-X branch unreachable. Preserve that observation.
    const auto previous_hp = health.internal_hp;
    if (armor) {
        damage_vehicle_armor(health, *armor, 50 * amount);
        if (health.armor_hp[*armor] < 0) {
            damage_vehicle_internal(health, -health.armor_hp[*armor]);
            health.armor_hp[*armor] = 0;
        }
    }
    const auto elimination = receiver.notify_internal_damage(previous_hp);
    if (elimination != Result::ok)
        return elimination;
    const bool guard = receiver.combat() && receiver.combat()->contact_weapon();
    const auto result = receiver.apply_damage_visual_contact(
        {force, point, receiver.state().position, receiver.transform().basis[2],
         receiver.state().heading_degrees, p.mass_kg, p.width, p.length, invulnerable,
         health.dying_timer != 0, guard});
    return result;
}
bool resolve_vehicle_collision(VehicleDynamics& a, VehicleDynamics& b,
                               const VehicleState& previous_a, const VehicleState& previous_b,
                               float dt, std::vector<RaceDebugLine>* debug,
                               float* remaining_seconds, const VehiclePairDamageContext* context) {
    const auto damage_one = [&](VehicleDynamics& receiver, const VehicleDynamics& attacker,
                                Vec3 force, Vec3 point, bool fallback, bool invulnerable,
                                bool player) {
        const auto* weapon = attacker.combat() ? attacker.combat()->contact_weapon() : nullptr;
        if (weapon && contact_weapon_front_hit(attacker.transform().basis[2],
                                               attacker.state().position, point)) {
            Vec3 modified;
            if (weapon->modify_contact_force(force, modified) == Result::ok)
                force = modified;
        }
        (void)fallback;
        const auto result = apply_vehicle_collision_damage(receiver, force, point, invulnerable,
                                                           player, context->difficulty);
        if (context->damage_visual_result && result != Result::ok)
            *context->damage_visual_result = result;
    };
    const auto damage = [&](Vec3 force, Vec3 point, bool fallback) {
        if (!context)
            return;
        if (!fallback) {
            if (a.equipment().dying_timer != 0)
                a.note_disabled_pair_contact();
            if (b.equipment().dying_timer != 0)
                b.note_disabled_pair_contact();
        }
        damage_one(a, b, force, point, fallback, context->a_invulnerable, context->a_player);
        damage_one(b, a, scale(force, -1), point, fallback, context->b_invulnerable,
                   context->b_player);
    };
    const auto effect = [&](Vec3 direction, Vec3 point, bool owner_b) {
        auto& owner = owner_b ? b : a;
        point.y = owner.state().position.y + owner.parameters().body_local_position.y;
        direction = scale(direction, owner_b ? -1.f : 1.f);
        const auto magnitude = modulus(direction);
        direction = magnitude != 0 ? scale(direction, 1 / magnitude) : Vec3{1, 0, 0};
        owner.append_particle_impact({point, direction, 1.5f});
    };
    const auto contact =
        solve_vehicle_collision(a.parameters(), a.collision_state(), previous_a, b.parameters(),
                                b.collision_state(), previous_b, dt, damage, effect);
    if (contact.hit) {
        if (remaining_seconds)
            *remaining_seconds = contact.removed_seconds;
        a.refresh_after_pair_collision();
        b.refresh_after_pair_collision();
        auto& sound_owner = contact.sound_on_b ? b : a;
        auto point = contact.point;
        point.y = sound_owner.state().position.y + sound_owner.parameters().body_local_position.y;
        sound_owner.append_pair_collision_event(contact.impact_strength, point);
    }
    if (contact.hit && debug) {
        auto point = contact.point;
        point.y = (a.state().position.y + b.state().position.y) * .5f;
        debug->push_back({point, add(point, scale(contact.normal, 3)), RaceDebugKind::collision, -1,
                          false, true});
    }
    return contact.hit;
}
} // namespace roadwars
