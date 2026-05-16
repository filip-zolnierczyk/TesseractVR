#version 450


vec2 positions[3] = vec2[](
    vec2(-1, -1),
    vec2(3, -1),
    vec2(-1, 3)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec2 uv;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    uv = positions[gl_VertexIndex] * 0.5 + 0.5;
}
