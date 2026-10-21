extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func valid_camera(host) -> bool:
    var eye: Vector3 = host.get_camera_position()
    return eye.is_finite() and host.get_camera_far_distance() > 1 and is_equal_approx(host.get_camera_far_distance(), host.get_view_distance_at(eye))

func run_test() -> void:
    var previous_mouse_mode = Input.mouse_mode
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if DisplayServer.get_name() != "headless":
        host.notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
        if Input.mouse_mode != previous_mouse_mode:
            push_error("Focus loss did not release mouse capture")
            quit(1)
            return
        host.notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
        if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
            push_error("Focus return did not restore mouse capture")
            quit(1)
            return
    for step in range(120):
        if not host.drive_test_step(1.0 / 60.0, 0, 0, 0, false):
            quit(1)
            return
    var before: Vector3 = host.get_camera_position()
    host.orbit_test_input(240, 50)
    host.drive_test_step(1.0 / 60.0, 0, 0, 0, false)
    var yaw: float = host.get_camera_orbit_yaw()
    var held_heading: float = host.get_camera_heading()
    if not valid_camera(host) or absf(yaw) < 30 or host.get_camera_position().distance_to(before) < 1:
        push_error("Mouse input did not orbit recovered camera")
        quit(1)
        return
    for step in range(150):
        host.drive_test_step(1.0 / 60.0, 0, 0, 0, false)
        if not valid_camera(host):
            quit(1)
            return
    if host.get_camera_orbit_state() != 1 or not is_equal_approx(held_heading, host.get_camera_heading()):
        push_error("Orbit returned before its three-second idle threshold")
        quit(1)
        return
    for step in range(60):
        host.drive_test_step(1.0 / 60.0, 0, 0, 0, false)
    if host.get_camera_orbit_state() != 2 or absf(host.get_camera_orbit_yaw()) >= absf(yaw) or absf(host.get_camera_orbit_yaw()) < 1:
        push_error("Orbit return did not remain gradual")
        quit(1)
        return
    for step in range(300):
        host.drive_test_step(1.0 / 60.0, 0, 0, 0, false)
    if host.get_camera_orbit_yaw() != 0 or not valid_camera(host):
        push_error("Orbit did not restore the original camera")
        quit(1)
        return
    # Manual hold must keep world heading while the vehicle and baseline turn.
    host.orbit_test_input(180, 0)
    host.drive_test_step(1.0 / 60.0, 0, 0, 0, false)
    held_heading = host.get_camera_heading()
    var vehicle_heading: float = host.get_vehicle_heading()
    for step in range(150):
        if not host.drive_test_step(1.0 / 60.0, 1, 1, 0, false) or host.get_camera_orbit_state() != 1 or absf(wrapf(host.get_camera_heading() - held_heading, -180, 180)) > 0.001:
            push_error("Manual camera followed the turning car before idle timeout")
            quit(1)
            return
    if absf(host.get_vehicle_heading() - vehicle_heading) < 1:
        push_error("Camera hold fixture did not turn the vehicle")
        quit(1)
        return
    for step in range(45):
        host.drive_test_step(1.0 / 60.0, 0, 0, 1, false)
    if host.get_camera_orbit_state() != 2:
        push_error("Manual hold did not enter returning state")
        quit(1)
        return
    var returning_heading: float = host.get_camera_heading()
    host.orbit_test_input(1, 0)
    host.drive_test_step(1.0 / 60.0, 0, 0, 1, false)
    if host.get_camera_orbit_state() != 1 or absf(wrapf(host.get_camera_heading() - returning_heading, -180, 180) - 0.15) > 0.01:
        push_error("Mouse interruption of return jumped to the original camera")
        quit(1)
        return
    for step in range(120):
        host.drive_test_step(1.0 / 60.0, 0, 0, 1, false)
    if host.get_camera_orbit_state() != 1 or not valid_camera(host):
        push_error("Interrupted return failed to restart the idle hold")
        quit(1)
        return
    await process_frame
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture="):
                root.get_texture().get_image().save_png(argument.trim_prefix("--capture="))
    print("ROADWARS_CAMERA_SMOKE_OK: recovered chase; mouse orbit; three-second idle; gradual return; actual-eye view distance")
    root.remove_child(host)
    if Input.mouse_mode != previous_mouse_mode:
        push_error("Scene exit retained mouse capture")
        quit(1)
        return
    if host.get_live_rids() != 0:
        quit(1)
        return
    host.free()
    quit(0)
