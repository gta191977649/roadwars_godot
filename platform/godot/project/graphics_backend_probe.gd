extends SceneTree
# Platform-only GPU startup fixture. No original assets or game startup is executed.
func _initialize() -> void:
    call_deferred("probe")

func probe() -> void:
    var shader := RenderingServer.shader_create()
    RenderingServer.shader_set_code(shader, "shader_type spatial; render_mode unshaded; void fragment() { ALBEDO = vec3(0.25, 0.5, 0.75); }")
    var material := RenderingServer.material_create()
    RenderingServer.material_set_shader(material, shader)
    var mesh := RenderingServer.mesh_create()
    var arrays := []
    arrays.resize(RenderingServer.ARRAY_MAX)
    arrays[RenderingServer.ARRAY_VERTEX] = PackedVector3Array([Vector3(-1,-1,-2), Vector3(0,1,-2), Vector3(1,-1,-2)])
    RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, arrays)
    RenderingServer.mesh_surface_set_material(mesh, 0, material)
    var scenario := RenderingServer.scenario_create()
    var camera := RenderingServer.camera_create()
    RenderingServer.viewport_set_scenario(root.get_viewport_rid(), scenario)
    RenderingServer.viewport_attach_camera(root.get_viewport_rid(), camera)
    var instance := RenderingServer.instance_create2(mesh, scenario)
    for frame in range(4):
        await process_frame
        await RenderingServer.frame_post_draw
    var image := root.get_texture().get_image()
    var rendered := false
    if image != null and not image.is_empty():
        var center := image.get_pixel(image.get_width() / 2, image.get_height() / 2)
        # Require the known blue/green/red ordering, rather than accepting a cleared viewport.
        rendered = center.b > center.g + 0.06 and center.g > center.r + 0.06 and center.r > 0.1
    if rendered:
        for argument in OS.get_cmdline_user_args():
            if argument.begins_with("--graphics-probe-output="):
                var file := FileAccess.open(argument.trim_prefix("--graphics-probe-output="), FileAccess.WRITE)
                if file:
                    file.store_string(RenderingServer.get_current_rendering_driver_name())
                    file.close()
    RenderingServer.viewport_set_scenario(root.get_viewport_rid(), RID())
    RenderingServer.viewport_attach_camera(root.get_viewport_rid(), RID())
    for rid in [instance, mesh, material, shader, camera, scenario]:
        RenderingServer.free_rid(rid)
    quit()
