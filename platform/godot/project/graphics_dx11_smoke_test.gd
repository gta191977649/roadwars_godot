extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    if RenderingServer.get_current_rendering_driver_name() != "opengl3_angle":
        push_error("Direct3D 11 must use the actual ANGLE driver")
        quit(1)
        return
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if host.get_graphics_state().active_backend != 4 or not host.graphics_test_apply({"backend": 4}, true):
        push_error("Cannot save Direct3D 11 startup settings")
        quit(1)
        return
    var config := ConfigFile.new()
    if config.load("user://graphics.cfg") != OK or config.get_value("rendering", "renderer/rendering_method.windows") != "gl_compatibility" or config.get_value("rendering", "gl_compatibility/driver.windows") != "opengl3_angle" or host.get_graphics_state().restart_required:
        push_error("Direct3D 11 renderer/driver settings do not agree")
        quit(1)
        return
    root.remove_child(host)
    host.free()
    print("ROADWARS_GRAPHICS_DX11_OK")
    quit()
