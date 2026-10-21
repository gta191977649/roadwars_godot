shader_type spatial;
render_mode cull_back
#ifdef RW_UNSHADED
    ,unshaded
#else
    ,vertex_lighting,diffuse_lambert,specular_disabled
#endif
#ifdef RW_DEPTH_WRITE
    ,depth_draw_always
#else
    ,depth_draw_never
#endif
#ifdef RW_NO_DEPTH_TEST
    ,depth_test_disabled
#endif
#ifndef RW_FOG
    ,fog_disabled
#endif
#ifdef RW_ADDITIVE
    ,blend_add
#endif
;

uniform sampler2D diffuse_map : source_color, filter_linear, repeat_enable;
uniform vec4 tint : source_color = vec4(1.0);
#ifdef RW_FOG
uniform vec3 original_fog_color : source_color;
uniform float original_fog_start;
uniform float original_fog_end;
varying float original_fog_amount;
#endif

#include "billboard_basis.gdshaderinc"

void vertex() {
#ifdef RW_BILLBOARD_Y
    vec3 right = -billboard_camera_right(INV_VIEW_MATRIX);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 back = cross(right, up);
    vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz),
                      length(MODEL_MATRIX[2].xyz));
    // 004dce9b: original heading + 180. Local Z was reflected on mesh upload.
    // Both X/Z flips preserve original atlas handedness and clockwise front faces.
    MODELVIEW_MATRIX = VIEW_MATRIX * mat4(vec4(right * scale.x, 0.0),
        vec4(up * scale.y, 0.0), vec4(back * scale.z, 0.0), MODEL_MATRIX[3]);
    MODELVIEW_NORMAL_MATRIX = transpose(inverse(mat3(MODELVIEW_MATRIX)));
#endif
#ifdef RW_FOG
    float depth = -(MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).z;
    original_fog_amount = clamp((depth - original_fog_start) /
        (original_fog_end - original_fog_start), 0.0, 1.0);
#endif
}

vec3 decode_color(vec3 color) {
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), color));
}

void fragment() {
    vec4 color = texture(diffuse_map, UV) * tint;
    color.rgb *= decode_color(COLOR.rgb);
    color.a *= COLOR.a;
#ifdef RW_OPAQUE_ALPHA_ONLY
    if (color.a < 0.99999) discard;
#else
    if (color.a <= 0.0) discard;
#endif
    ALBEDO = color.rgb;
    ROUGHNESS = 1.0;
#ifdef RW_FOG
    FOG = vec4(original_fog_color, original_fog_amount);
#endif
#ifdef RW_ALPHA
    ALPHA = color.a;
#endif
}
