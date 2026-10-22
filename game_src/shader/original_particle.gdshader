shader_type spatial;
render_mode unshaded,cull_disabled,depth_draw_never,fog_disabled
#ifdef RW_TIRE
    ,blend_mix
#else
    ,blend_add
#endif
;
uniform sampler2D original_texture : source_color, filter_linear, repeat_enable;
uniform vec2 atlas_v = vec2(.218, .609);

#include "billboard_basis.gdshaderinc"

void vertex() {
    vec2 local_vertex = VERTEX.xy * INSTANCE_CUSTOM.x;
#ifdef RW_CUSTOM_GEOMETRY
    if (INSTANCE_CUSTOM.w > 0.5) {
        int slot = int(INSTANCE_CUSTOM.z);
        vec4 ab = slot_triangle_ab[slot];
        vec2 c = slot_triangle_c[slot].xy;
        local_vertex = ab.xy * (1.0 - UV.x - UV.y) + ab.zw * UV.x + c * UV.y;
    }
#endif
    VERTEX = billboard_camera_right(INV_VIEW_MATRIX) * local_vertex.x +
        vec3(0.0, 1.0, 0.0) * local_vertex.y;
    int frame = int(INSTANCE_CUSTOM.y);
    vec2 a;
    vec2 b;
    vec2 c;
    if (frame == 0) {
        a = vec2(0.0, atlas_v.x); b = vec2(0.0, atlas_v.y); c = vec2(.391, atlas_v.y);
    } else if (frame == 1) {
        a = vec2(.391, atlas_v.y); b = vec2(.391, atlas_v.x); c = vec2(0.0, atlas_v.x);
    } else if (frame == 2) {
        a = vec2(.391, atlas_v.x); b = vec2(.391, atlas_v.y); c = vec2(.782, atlas_v.y);
    } else {
        a = vec2(.782, atlas_v.y); b = vec2(.782, atlas_v.x); c = vec2(.391, atlas_v.x);
    }
    UV = a * (1.0 - UV.x - UV.y) + b * UV.x + c * UV.y;
}

void fragment() {
    vec4 color = texture(original_texture, UV) * COLOR;
    if (color.a <= 0.0) discard;
    ALBEDO = color.rgb;
    ALPHA = color.a;
}
