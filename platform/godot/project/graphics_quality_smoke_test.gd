extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    # No scene startup or installation assets are required by the renderer fixture.
    var host = ClassDB.instantiate("GameHost")
    var valid: bool = host.test_graphics_quality()
    host.free()
    if not valid:
        push_error("Graphics quality texture, sampler, paint update or lifetime regression")
        quit(1)
        return
    print("Graphics quality fixture passed")
    quit()
