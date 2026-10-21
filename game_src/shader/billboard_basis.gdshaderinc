// Screen-aligned around world Y. Use the current render pass, including camera interpolation.
vec3 billboard_camera_right(mat4 inverse_view) {
    vec3 right = vec3(inverse_view[0].x, 0.0, inverse_view[0].z);
    return dot(right, right) > 0.00000001 ? normalize(right) : vec3(1.0, 0.0, 0.0);
}
