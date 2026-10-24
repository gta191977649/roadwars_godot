extends SceneTree

var capture := AudioEffectCapture.new()
var failed := false

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        push_error(message)
        failed = true
    return value

func measure(voice: AudioStreamPlayer3D, point: Vector3) -> Vector2:
    voice.global_position = point
    await create_timer(0.2).timeout
    capture.clear_buffer()
    await create_timer(0.15).timeout
    var samples := capture.get_buffer(capture.get_frames_available())
    if samples.is_empty():
        require(false, "Native audio capture returned no mixed frames")
        return Vector2.ZERO
    var energy := Vector2.ZERO
    for sample in samples:
        energy += Vector2(sample.x * sample.x, sample.y * sample.y)
    return Vector2(sqrt(energy.x / samples.size()), sqrt(energy.y / samples.size()))

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    host.present_test_frame()
    host.begin_sfx_fixture()
    host.sfx_fixture_tire(true)
    var voice := host.get_node("OriginalTireSkidVoice") as AudioStreamPlayer3D
    var listener := host.get_node("RaceAudioListener") as AudioListener3D
    if require(voice != null and listener != null, "Race adapter did not create spatial voice/listener"):
        require(listener.is_current(), "Race camera listener is not current")
        var presented: Transform3D = host.get_presentation_state()["camera"]["presented"]
        require(listener.global_transform.is_equal_approx(presented),
                "Audio listener differs from the presented rendering camera")
        require(voice.attenuation_model == AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE,
                "Unexpected spatial attenuation model")
        require(voice.unit_size == 15 and voice.max_distance == 125,
                "Unexpected spatial distance scale")
        require(is_equal_approx(db_to_linear(voice.max_db), voice.volume_linear),
                "Near-distance amplification bypasses the configured SFX source gain")
        # The production adapter must convert the actual original source position once.
        var car: Vector3 = host.get_vehicle_position()
        require(voice.global_position.is_equal_approx(Vector3(car.x, car.y, -car.z)),
                "Spatial source coordinate conversion differs from rendering")
        voice.volume_linear = 0.5
        voice.max_db = linear_to_db(0.5)
        capture.buffer_length = 1
        AudioServer.add_bus_effect(0, capture)
        var pose: Transform3D = listener.global_transform
        var near_level := await measure(voice, pose * Vector3(0, 0, -5))
        var far_level := await measure(voice, pose * Vector3(0, 0, -80))
        var outside := await measure(voice, pose * Vector3(0, 0, -140))
        var left := await measure(voice, pose * Vector3(-20, 0, -10))
        var right := await measure(voice, pose * Vector3(20, 0, -10))
        require(near_level.length() > 0.001, "Near spatial voice did not reach the mixer")
        require(far_level.length() < near_level.length() * 0.3,
                "Distance did not reduce the real mixed signal")
        require(outside.length() < 0.0001, "Out-of-range spatial voice remained audible")
        require(left.x > left.y * 1.2 and right.y > right.x * 1.2,
                "Spatial left/right panning is reversed or absent")
        voice.stream_paused = true
        var paused := await measure(voice, pose * Vector3(0, 0, -5))
        require(paused.length() < 0.0001, "Paused spatial loop remained audible")
        voice.stream_paused = false
        print("SPATIAL_MIX near=", near_level, " far=", far_level,
              " outside=", outside, " left=", left, " right=", right)
        AudioServer.remove_bus_effect(0, AudioServer.get_bus_effect_count(0) - 1)
        host.sfx_fixture_tire(false)
        host.sfx_fixture_collision(false, 1)
        var collision: Node = host.get_node("OriginalCrashVoice_0")
        require(collision is AudioStreamPlayer3D, "Collision voice is not spatial")
        await create_timer(0.15).timeout
        host.start_fog_fixture()
        require(not host.has_node("OriginalTireSkidVoice") and
                host.get_active_collision_voices() == 0, "Spatial voices survived fixture teardown")
    root.remove_child(host)
    host.free()
    await create_timer(0.2).timeout
    if not failed:
        print("ROADWARS_SPATIAL_AUDIO_SMOKE_OK")
    quit(1 if failed else 0)
