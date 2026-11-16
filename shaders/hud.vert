#version 450

// HUD jest rysowany w przestrzeni ekranu (pixele), bez macierzy view/proj,
// wiec jest nieruchomy wzgledem kamery/glowy uzytkownika.
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec2 resolution;
    float time;
    float w_offset;
} ubo;

layout(location = 0) out vec2 localPx;      // pozycja w pikselach wewnatrz paska (od lewego gornego rogu)
layout(location = 1) out vec2 barSizePx;
layout(location = 2) out flat int isPanel;

void main() {
    vec2 res = ubo.resolution;

    // Pasek na dole: 60% szerokosci (50% * 1.2), 8% wysokosci, margines 2% od dolu + 2 wysokosci paska
    vec2 size = vec2(res.x * 0.6, res.y * 0.08);
    vec2 origin = vec2((res.x - size.x) * 0.5, res.y - size.y - res.y * 0.02 - 2.0 * size.y);

    // Panel ustawien (wierzcholki 6-11): biały prostokat 40% x 40% ekranu na srodku
    isPanel = gl_VertexIndex >= 6 ? 1 : 0;
    if (isPanel == 1) {
        size = vec2(res.x * 0.4, res.y * 0.4);
        origin = (res - size) * 0.5;
    }

    const vec2 corners[6] = vec2[](
        vec2(0, 0), vec2(1, 0), vec2(1, 1),
        vec2(0, 0), vec2(1, 1), vec2(0, 1)
    );
    vec2 c = corners[gl_VertexIndex % 6];
    vec2 px = origin + c * size;

    gl_Position = vec4(px / res * 2.0 - 1.0, 0.0, 1.0);
    localPx = c * size;
    barSizePx = size;
}
