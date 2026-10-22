extends RefCounted

static func snapshot(host) -> Array:
    return [host.get_vehicle_position(),host.get_vehicle_heading(),host.get_vehicle_speed(),host.get_crash_sound_count(),host.get_tire_sound_start_count(),host.get_particle_count(),host.get_skid_vertex_count(),host.get_simulation_ticks()]

static func verify_presentation(host, before: Dictionary) -> bool:
    var pending: Dictionary = host.get_presentation_state()
    # Simulation may change snapshots, but only presentation may submit transforms.
    for id in before.instances:
        if pending.instances.has(id) and pending.instances[id].presented != before.instances[id].presented:
            push_error("Simulation submitted a raw instance transform before presentation")
            return false
    if pending.camera.presented != before.camera.presented:
        push_error("Simulation submitted a raw camera transform before presentation")
        return false
    var physics = snapshot(host)
    host.present_test_frame()
    var presented: Dictionary = host.get_presentation_state()
    var alpha: float = host.get_simulation_alpha()
    for id in pending.instances:
        var pose: Dictionary = presented.instances[id]
        var expected: Transform3D = pose.previous.interpolate_with(pose.current, alpha)
        if not pose.presented.is_equal_approx(expected):
            push_error("Instance presentation differs from its simulation interval")
            return false
    var camera: Dictionary = presented.camera
    if not camera.presented.is_equal_approx(camera.previous.interpolate_with(camera.current, alpha)):
        push_error("Camera presentation differs from the shared simulation interval")
        return false
    if not is_equal_approx(presented.presented_fov, lerpf(presented.previous_fov, presented.current_fov, alpha)):
        push_error("Camera projection differs from the shared simulation interval")
        return false
    host.present_test_frame()
    if physics != snapshot(host) or presented != host.get_presentation_state():
        push_error("Presentation mutated physics or its own simulation snapshots")
        return false
    return true


# Vulkan can differ by a few output codes at a few triangle-edge pixels even when
# all submitted transforms are identical. Keep the geometry/state checks exact.
static func frozen_pixels_match(before: Image, after: Image) -> bool:
    if before.get_size() != after.get_size() or before.get_format() != after.get_format():
        return false
    var a = before.get_data()
    var b = after.get_data()
    if a == b:
        return true
    var channels = 4 if before.get_format() == Image.FORMAT_RGBA8 else 3
    if before.get_format() not in [Image.FORMAT_RGB8, Image.FORMAT_RGBA8]:
        return false
    var changed_pixels = 0
    for offset in range(0, a.size(), channels):
        var changed = false
        for channel in range(channels):
            var difference = absi(a[offset + channel] - b[offset + channel])
            if difference > 2:
                return false
            changed = changed or difference != 0
        if changed:
            changed_pixels += 1
            if changed_pixels > 16:
                return false
    return true
