extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    if RenderingServer.get_current_rendering_driver_name() != "opengl3":
        push_error("The actual native OpenGL driver is required")
        quit(1)
        return
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if host.get_graphics_state().backends != [1, 5]:
        push_error("Disabled renderers remained selectable")
        quit(1)
        return
    if host.get_graphics_state().active_backend != 5 or not host.graphics_test_apply({"backend": 5}, true):
        push_error("Cannot save OpenGL startup settings")
        quit(1)
        return
    for disabled in [0, 2, 3, 4]:
        if host.graphics_test_apply({"backend": disabled}, true):
            push_error("Disabled renderer accepted through settings API")
            quit(1)
            return
    var config := ConfigFile.new()
    if config.load("user://graphics.cfg") != OK or config.get_value("rendering", "renderer/rendering_method.windows") != "gl_compatibility" or config.get_value("rendering", "gl_compatibility/driver.windows") != "opengl3" or host.get_graphics_state().restart_required:
        push_error("OpenGL renderer/driver settings do not agree")
        quit(1)
        return
    root.remove_child(host)
    host.free()
    print("ROADWARS_GRAPHICS_OPENGL_OK")
    quit()
