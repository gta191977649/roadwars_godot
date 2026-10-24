extends SceneTree

var failed := false

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        failed = true
    return value

func press(host: Node, key: String) -> void:
    require(host.frontend_test_step(0, ""), "Sound input release failed")
    require(host.frontend_test_step(0.06, key), "Sound input failed: " + key)

func run_test() -> void:
    var existed := FileAccess.file_exists("user://sound.cfg")
    var original := FileAccess.get_file_as_bytes("user://sound.cfg") if existed else PackedByteArray()
    # Save a known candidate so the complete native host load/save path is exercised.
    var settings := ConfigFile.new()
    settings.set_value("sound", "version", 1)
    settings.set_value("sound", "MUSIC", 1.0)
    settings.set_value("sound", "SFX", 0.75)
    settings.set_value("sound", "INGAMEMUSIC", 0.5)
    settings.set_value("sound", "PLAYMUSICFROMCD", false)
    settings.set_value("sound", "USE3DSOUNDHARDWARE", false)
    settings.set_value("sound", "SOUNDCHANNELS", 20)
    require(settings.save("user://sound.cfg") == OK, "Cannot initialize sound smoke preferences")
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    await create_timer(0.2).timeout
    require(host.preview_frontend_screen("sound", 1, ""), "Cannot open sound options")
    await create_timer(0.2).timeout
    if DisplayServer.get_name() != "headless":
        await RenderingServer.frame_post_draw
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--capture-dir="):
                require(root.get_texture().get_image().save_png(
                    argument.trim_prefix("--capture-dir=").path_join("sound_options.png")) == OK,
                    "Cannot capture sound options")
    var music: AudioStreamPlayer = host.get_node("OriginalFrontendMusic")
    require(is_equal_approx(music.volume_linear, 1), "Saved MUSIC did not load")
    press(host, "down")
    press(host, "right")
    require(is_zero_approx(music.volume_linear), "100 -> 0 MUSIC wrap did not affect native voice")
    press(host, "left")
    require(is_equal_approx(music.volume_linear, 1), "0 -> 100 MUSIC wrap failed")
    press(host, "down")
    press(host, "right")
    press(host, "down")
    press(host, "left")
    press(host, "back")
    await create_timer(0.2).timeout
    require(settings.load("user://sound.cfg") == OK, "Cannot read saved sound options")
    require(is_equal_approx(float(settings.get_value("sound", "SFX")), 0.8), "SFX was not saved")
    require(is_equal_approx(float(settings.get_value("sound", "INGAMEMUSIC")), 0.45),
            "Race music was not saved")
    # A fresh host must consume the persistent values, not only retain menu row state.
    root.remove_child(host)
    host.free()
    await create_timer(0.15).timeout
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    await create_timer(0.2).timeout
    require(host.preview_frontend_screen("sound", 1, ""), "Cannot reopen sound options")
    await create_timer(0.2).timeout
    press(host, "back")
    await create_timer(0.2).timeout
    require(settings.load("user://sound.cfg") == OK and
            is_equal_approx(float(settings.get_value("sound", "SFX")), 0.8),
            "Reopening sound options lost persisted settings")
    root.remove_child(host)
    host.free()
    await create_timer(0.15).timeout
    if existed:
        var restore := FileAccess.open("user://sound.cfg", FileAccess.WRITE)
        restore.store_buffer(original)
        restore.close()
    else:
        DirAccess.remove_absolute("user://sound.cfg")
    if not failed:
        print("ROADWARS_SOUND_OPTIONS_SMOKE_OK: live music; wrap; save; fresh host reload; teardown")
    quit(1 if failed else 0)
