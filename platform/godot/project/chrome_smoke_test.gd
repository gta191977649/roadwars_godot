extends SceneTree

const PIXEL_TOLERANCE = 0.035
const CAPTURE = "res://../../../build/chrome_fixture.png"

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    var initial_rids = -1
    for cycle in range(3):
        var fixture: Dictionary = host.start_chrome_fixture()
        if not fixture.has("samples") or fixture["samples"].is_empty():
            push_error("Chrome fixture did not return pixel samples in cycle %d" % cycle)
            quit(1)
            return
        await process_frame
        await RenderingServer.frame_post_draw
        var image = root.get_texture().get_image()
        if image == null or image.is_empty():
            push_error("Chrome fixture has no rendered viewport image")
            quit(1)
            return
        for sample in fixture["samples"]:
            var position: Vector2 = sample["position"]
            var expected: Color = sample["color"]
            var label: String = sample["label"]
            var x = clampi(int(position.x * image.get_width()), 0, image.get_width() - 1)
            var y = clampi(int(position.y * image.get_height()), 0, image.get_height() - 1)
            var actual = image.get_pixel(x, y)
            if (abs(actual.r - expected.r) > PIXEL_TOLERANCE or
                    abs(actual.g - expected.g) > PIXEL_TOLERANCE or
                    abs(actual.b - expected.b) > PIXEL_TOLERANCE):
                push_error("Chrome pixel mismatch '%s' cycle %d at (%d,%d): expected %s actual %s" %
                        [label, cycle, x, y, expected, actual])
                quit(1)
                return
        var live_rids: int = host.get_live_rids()
        if initial_rids < 0:
            initial_rids = live_rids
        if live_rids <= 0 or live_rids != initial_rids:
            push_error("Chrome fixture RID count changed in cycle %d: expected %d actual %d" %
                    [cycle, initial_rids, live_rids])
            quit(1)
            return
        if cycle == 2 and image.save_png(CAPTURE) != OK:
            push_error("Could not save chrome fixture capture to %s" % CAPTURE)
            quit(1)
            return
    root.remove_child(host)
    if host.get_live_rids() != 0:
        push_error("Chrome fixture retained %d RIDs after exit" % host.get_live_rids())
        host.free()
        quit(1)
        return
    host.free()
    print("ROADWARS_CHROME_SMOKE_OK: chrome pixels and three fixture lifecycles verified")
    quit(0)
