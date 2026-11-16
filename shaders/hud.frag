#version 450

layout(location = 0) in vec2 localPx;
layout(location = 1) in vec2 barSizePx;
layout(location = 2) in flat int isPanel;
layout(location = 0) out vec4 outColor;

// Wspolny uklad UBO z 09_shader_base.frag (shapeId ustawiany z Application przez przyciski kontrolera)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec2 resolution;
    float time;
    float w_offset;
    float nearPlane;
    float aXY;
    float aXZ;
    float aXW;
    float aYZ;
    float aYW;
    float aZW;
    float shapeId;
    float colorMode;
    float globalScale;
    float settingSelected;
} ubo;

// Nazwy figur (kody ASCII, wiersz na figure, dopelnione spacjami do 14 znakow); kolejnosc jak shapeId
const int NAME_STRIDE = 14;
const int NAME_LENS[7] = int[](3, 6, 7, 11, 9, 5, 14);
const int NAMES[98] = int[](
    66, 79, 88, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,                // BOX
    83, 80, 72, 69, 82, 69, 32, 32, 32, 32, 32, 32, 32, 32,                // SPHERE
    67, 65, 80, 83, 85, 76, 69, 32, 32, 32, 32, 32, 32, 32,                // CAPSULE
    82, 79, 85, 78, 68, 69, 68, 32, 66, 79, 88, 32, 32, 32,                // ROUNDED BOX
    69, 76, 76, 73, 80, 83, 79, 73, 68, 32, 32, 32, 32, 32,                // ELLIPSOID
    84, 79, 82, 85, 83, 32, 32, 32, 32, 32, 32, 32, 32, 32,                // TORUS
    67, 82, 79, 83, 83, 45, 80, 79, 76, 89, 84, 79, 80, 69                 // CROSS-POLYTOPE
);
// Czcionka 5x7 (tylko litery uzywane w nazwach figur, '-' i spacja); kazdy wiersz to 5 bitow
int glyphRow(int ch, int y) {
    switch (ch) {
    case 65: return int[7](0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11)[y]; // A
    case 66: return int[7](0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E)[y]; // B
    case 67: return int[7](0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E)[y]; // C
    case 68: return int[7](0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E)[y]; // D
    case 69: return int[7](0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F)[y]; // E
    case 72: return int[7](0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11)[y]; // H
    case 73: return int[7](0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E)[y]; // I
    case 76: return int[7](0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F)[y]; // L
    case 78: return int[7](0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11)[y]; // N
    case 79: return int[7](0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E)[y]; // O
    case 80: return int[7](0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10)[y]; // P
    case 82: return int[7](0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11)[y]; // R
    case 83: return int[7](0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E)[y]; // S
    case 84: return int[7](0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04)[y]; // T
    case 85: return int[7](0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E)[y]; // U
    case 88: return int[7](0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11)[y]; // X
    case 89: return int[7](0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04)[y]; // Y
    case 45: return int[7](0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00)[y]; // -
    case 70: return int[7](0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10)[y]; // F
    case 77: return int[7](0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11)[y]; // M
    case 37: return int[7](0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03)[y]; // %
    case 48: return int[7](0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E)[y]; // 0
    case 49: return int[7](0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E)[y]; // 1
    case 50: return int[7](0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F)[y]; // 2
    case 51: return int[7](0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E)[y]; // 3
    case 52: return int[7](0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02)[y]; // 4
    case 53: return int[7](0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E)[y]; // 5
    case 54: return int[7](0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E)[y]; // 6
    case 55: return int[7](0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08)[y]; // 7
    case 56: return int[7](0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E)[y]; // 8
    case 57: return int[7](0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C)[y]; // 9
    default: return 0;                                                   // spacja
    }
}

// Teksty panelu ustawien (kody ASCII w jednej tablicy; offset/dlugosc podawane osobno)
const int PANEL_STR[47] = int[](
    67, 79, 76, 79, 82, 32, 77, 79, 68, 69,                 // 0  COLOR MODE (10)
    83, 67, 65, 76, 69,                                     // 10 SCALE (5)
    78, 79, 82, 77, 65, 76, 83,                             // 15 NORMALS (7)
    70, 65, 67, 69, 83, 32, 52, 68,                         // 22 FACES 4D (8)
    70, 65, 67, 69, 83, 32, 83, 84, 65, 66, 76, 69,         // 30 FACES STABLE (12)
    83, 79, 76, 73, 68                                      // 42 SOLID (5)
);
const int COLOR_OFFSET[4] = int[](15, 22, 30, 42);
const int COLOR_LEN[4]    = int[](7, 8, 12, 5);

// Czy piksel p (w jednostkach "kropek" od lewego gornego rogu tekstu) nalezy do znaku nr cell
bool charHit(int ch, vec2 p, int cell) {
    if (p.y < 0.0 || p.y >= 7.0) return false;
    float x = p.x - float(cell) * 6.0;
    if (x < 0.0 || x >= 5.0) return false;
    return ((glyphRow(ch, int(p.y)) >> (4 - int(x))) & 1) == 1;
}

// Tekst z PANEL_STR; origin to lewy gorny rog tekstu wyrodkowanego w szerokosci w
bool tableTextHit(vec2 q, float w, float y0, float dotPx, int base, int len) {
    float textW = (float(len) * 6.0 - 1.0) * dotPx;
    vec2 p = (q - vec2((w - textW) * 0.5, y0)) / dotPx;
    if (p.x < 0.0) return false;
    int cell = int(floor(p.x / 6.0));
    if (cell >= len) return false;
    return charHit(PANEL_STR[base + cell], p, cell);
}

// Wartosc skali jako procent, np. "125%"
bool scaleTextHit(vec2 q, float w, float y0, float dotPx) {
    int percent = int(ubo.globalScale * 100.0 + 0.5);
    int digits = percent >= 100 ? 3 : 2;
    int len = digits + 1;
    float textW = (float(len) * 6.0 - 1.0) * dotPx;
    vec2 p = (q - vec2((w - textW) * 0.5, y0)) / dotPx;
    if (p.x < 0.0) return false;
    int cell = int(floor(p.x / 6.0));
    if (cell >= len) return false;
    int ch = 37; // '%'
    if (cell < digits) {
        int place = digits - 1 - cell;
        int div = place == 2 ? 100 : (place == 1 ? 10 : 1);
        ch = 48 + (percent / div) % 10;
    }
    return charHit(ch, p, cell);
}

// Panel ustawien: lista pozycji (nazwa + aktualny tryb); wybrana pozycja jest odwrocona
float settingsPanelColor(vec2 pp, vec2 panelSize) {
    const int ITEM_COUNT = 2;
    float itemH = panelSize.y * 0.26;
    float gap = panelSize.y * 0.04;
    float mX = panelSize.x * 0.05;
    float mY = panelSize.y * 0.05;
    float itemW = panelSize.x - 2.0 * mX;
    float border = max(2.0, itemH * 0.06);

    for (int i = 0; i < ITEM_COUNT; i++) {
        vec2 q = pp - vec2(mX, mY + float(i) * (itemH + gap));
        if (q.x < 0.0 || q.y < 0.0 || q.x >= itemW || q.y >= itemH) continue;

        bool selected = (i == int(ubo.settingSelected + 0.5));
        bool isBorder = q.x < border || q.y < border || q.x >= itemW - border || q.y >= itemH - border;
        if (isBorder) return 0.0;

        float nameDot = itemH * 0.30 / 7.0;
        float valueDot = itemH * 0.22 / 7.0;
        float nameY = itemH * 0.12;
        float valueY = itemH * 0.58;

        bool ink;
        if (i == 0) {
            int mode = clamp(int(ubo.colorMode + 0.5), 0, 3);
            ink = tableTextHit(q, itemW, nameY, nameDot, 0, 10)
               || tableTextHit(q, itemW, valueY, valueDot, COLOR_OFFSET[mode], COLOR_LEN[mode]);
        } else {
            ink = tableTextHit(q, itemW, nameY, nameDot, 10, 5)
               || scaleTextHit(q, itemW, valueY, valueDot);
        }
        // Wybrana pozycja: czarne tlo i biale znaki; pozostale odwrotnie
        return (selected != ink) ? 0.0 : 1.0;
    }
    return 1.0;
}

void main() {
    if (isPanel == 1) {
        outColor = vec4(vec3(settingsPanelColor(localPx, barSizePx)), 1.0);
        return;
    }
    const float DOT = 0.5 / 7.0; // tekst zajmuje 50% wysokosci paska

    int shape = clamp(int(ubo.shapeId + 0.5), 0, 6);
    int textLen = NAME_LENS[shape];

    float dot_ = barSizePx.y * DOT;
    float textW = (float(textLen) * 6.0 - 1.0) * dot_;
    vec2 p = (localPx - vec2((barSizePx.x - textW) * 0.5, barSizePx.y * 0.25)) / dot_;

    vec3 col = vec3(1.0);
    if (p.x >= 0.0 && p.y >= 0.0 && p.y < 7.0) {
        int cell = int(floor(p.x / 6.0));
        float inCell = p.x - float(cell) * 6.0;
        if (cell < textLen && inCell < 5.0) {
            if (((glyphRow(NAMES[shape * NAME_STRIDE + cell], int(p.y)) >> (4 - int(inCell))) & 1) == 1) {
                col = vec3(0.0);
            }
        }
    }
    outColor = vec4(col, 1.0);
}
