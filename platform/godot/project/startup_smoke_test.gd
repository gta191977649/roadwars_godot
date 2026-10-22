extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        quit(1)
    return value

func run_test() -> void:
    var expected = ""
    var fixture = ""
    var waiting = false
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--test-root="): expected = arg.trim_prefix("--test-root=")
        if arg.begins_with("--test-fixture="): fixture = arg.trim_prefix("--test-fixture=")
        if arg == "--test-wait": waiting = true
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    if DisplayServer.get_name() != "headless":
        var mode = "Debug" if "--debug" in OS.get_cmdline_user_args() else "Release"
        if not require(root.title.begins_with("RoadWars-RE (") and root.title.ends_with(" - " + mode + " | By Nurupo"), "Window title format differs"): return
    var state = host.get_startup_state()
    if not require(state.waiting == waiting, "Unexpected startup waiting state"): return
    if waiting:
        if not require(not state.initialized and host.get_simulation_ticks() == 0, "Initialized before asset selection"): return
        host.select_game_directory(fixture.path_join("missing"))
        if not require(host.get_startup_state().waiting, "Invalid selection was accepted"): return
        host.cancel_game_directory()
        if not require(host.get_startup_state().waiting, "Cancel lost the prompt"): return
        await process_frame
        host.select_game_directory(expected)
    if not require(host.get_startup_state().initialized, "Valid root did not initialize frontend"): return
    if not require(host.get_startup_state().root.replace("\\", "/") == expected.replace("\\", "/"), "Root precedence differs"): return
    if not fixture.is_empty():
        if not require(host.validate_game_directory_for_test(fixture).valid, "Case-aware Unicode fixture rejected"): return
        if not require(not host.validate_game_directory_for_test(fixture.path_join("missing")).valid, "Invalid fixture accepted"): return
    root.remove_child(host)
    host.free()
    await create_timer(0.6).timeout
    print("ROADWARS_STARTUP_SMOKE_OK")
    quit(0)
