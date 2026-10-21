extends SceneTree

func _initialize() -> void:
    call_deferred("run_test")

func run_test() -> void:
    var host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    host.start_billboard_fixture()
    var minimum_width = {0: 100000, 35: 100000, -35: 100000}
    var maximum_width = {0: 0, 35: 0, -35: 0}
    for yaw in [0, 45, 90, 135, 180, 225, 270, 315]:
        for pitch in [0, 35, -35]:
            host.set_billboard_fixture_camera(yaw, pitch)
            await process_frame
            await RenderingServer.frame_post_draw
            var image = root.get_texture().get_image()
            var red_count = 0
            var blue_count = 0
            var red_x = 0.0
            var blue_x = 0.0
            var left = image.get_width()
            var right = 0
            for y in range(image.get_height()):
                for x in range(image.get_width()):
                    var color = image.get_pixel(x, y)
                    if color.r > 0.7 and color.b < 0.2:
                        red_count += 1
                        red_x += x
                        left = mini(left, x)
                        right = maxi(right, x)
                    elif color.b > 0.7 and color.r < 0.2:
                        blue_count += 1
                        blue_x += x
                        left = mini(left, x)
                        right = maxi(right, x)
            if red_count < 200 or blue_count < 200 or red_x / max(red_count, 1) >= blue_x / max(blue_count, 1):
                push_error("Billboard lost front face or mirrored atlas at yaw=%s pitch=%s red=%s blue=%s" % [yaw, pitch, red_count, blue_count])
                quit(1)
                return
            minimum_width[pitch] = mini(minimum_width[pitch], right - left)
            maximum_width[pitch] = maxi(maximum_width[pitch], right - left)
            if yaw == 45 and pitch == 35:
                image.save_png("res://../../../build/billboard_fixture.png")
    for pitch in minimum_width:
        if maximum_width[pitch] - minimum_width[pitch] > 5:
            push_error("Billboard became edge-on while orbiting: pitch %s widths %s..%s" % [pitch, minimum_width[pitch], maximum_width[pitch]])
            quit(1)
            return
    root.remove_child(host)
    if host.get_live_rids() != 0:
        quit(1)
        return
    host.free()
    print("ROADWARS_BILLBOARD_SMOKE_OK: 24 views; original winding; atlas orientation; GPU camera alignment")
    quit(0)
