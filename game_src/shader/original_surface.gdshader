shader_type spatial;
render_mode cull_back
#ifdef RW_CHROME
    ,unshaded,fog_disabled
#else
#ifdef RW_UNSHADED
    ,unshaded
#else
    ,vertex_lighting,diffuse_lambert,specular_disabled
#endif
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
#ifndef RW_CHROME
    ,fog_disabled
#endif
#endif
#ifdef RW_ADDITIVE
    ,blend_add
#endif
;

uniform sampler2D diffuse_map : source_color, filter_linear, repeat_enable;
uniform vec4 tint : source_color = vec4(1.0);
uniform vec4 original_vertex_override = vec4(0.0, 0.0, 0.0, -1.0);
#ifdef RW_CHROME
// Raw encoded RGB is intentional: D3D7 ADD operates before gamma conversion.
uniform sampler2D chrome_map : filter_linear, repeat_enable;
uniform sampler2D chrome_base_map : filter_linear, repeat_enable;
uniform vec4 chrome_material_color = vec4(1.0);
uniform vec3 chrome_light_direction;
uniform float chrome_ambient;
uniform vec3 chrome_fog_color;
varying vec2 chrome_uv;
varying vec4 chrome_diffuse;
#endif
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
#ifdef RW_CHROME
    // CUSTOM0 retains raw normals; Godot's packed NORMAL stream loses their length.
    // 004d5425 uses world/view upper3x3, not inverse transpose or normalization.
    vec3 chrome_normal = mat3(MODELVIEW_MATRIX) * CUSTOM0.xyz;
    chrome_uv = vec2((chrome_normal.x + 1.0) * 0.5, (1.0 - chrome_normal.y) * 0.5);
#ifdef RW_UNSHADED
    chrome_diffuse = original_vertex_override.a >= 0.0 ? original_vertex_override : COLOR;
#else
    vec3 lit_normal = MODEL_NORMAL_MATRIX * CUSTOM0.xyz;
    float diffuse = max(dot(lit_normal, -chrome_light_direction), 0.0);
    chrome_diffuse = vec4(clamp(chrome_material_color.rgb * (chrome_ambient + diffuse),
                               vec3(0.0), vec3(1.0)), chrome_material_color.a);
#endif
#endif
}

vec3 decode_color(vec3 color) {
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), color));
}
void fragment() {
    vec4 color = texture(diffuse_map, UV) * tint;
    vec4 original_color = original_vertex_override.a >= 0.0 ? original_vertex_override : COLOR;
    color.rgb *= decode_color(original_color.rgb);
    color.a *= original_color.a;
#ifdef RW_CHROME
    // Single-pass stage1 ADD follows stage0 texture * Gouraud diffuse.
    vec3 encoded = clamp(texture(chrome_base_map, UV).rgb * chrome_diffuse.rgb +
                         texture(chrome_map, chrome_uv).rgb, vec3(0.0), vec3(1.0));
    color.a = texture(chrome_base_map, UV).a * chrome_diffuse.a;
#ifdef RW_FOG
    encoded = mix(encoded, chrome_fog_color, original_fog_amount);
#endif
    color.rgb = decode_color(encoded);
#endif
#ifdef RW_OPAQUE_ALPHA_ONLY
    if (color.a < 0.99999) discard;
#else
    if (color.a <= 0.0) discard;
#endif
    ALBEDO = color.rgb;
    ROUGHNESS = 1.0;
#ifdef RW_FOG
#ifndef RW_CHROME
    FOG = vec4(original_fog_color, original_fog_amount);
#endif
#endif
#ifdef RW_ALPHA
    ALPHA = color.a;
#endif
}
