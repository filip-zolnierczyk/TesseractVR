#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec2 resolution;
    float time;
    float w_offset;
    float aXY;
    float aXZ;
    float aXW;
    float aYZ;
    float aYW;
    float aZW;
} ubo;
// 4D plane rotations implemented as inplace transforms on vec4
vec4 rotXY(vec4 p, float a) {
    float c = cos(a), s = sin(a);
    return vec4(c*p.x - s*p.y, s*p.x + c*p.y, p.z, p.w);
}
vec4 rotXZ(vec4 p, float a) {
    float c = cos(a), s = sin(a);
    return vec4(c*p.x - s*p.z, p.y, s*p.x + c*p.z, p.w);
}
vec4 rotXW(vec4 p, float a) {
    float c = cos(a), s = sin(a);
    return vec4(c*p.x - s*p.w, p.y, p.z, s*p.x + c*p.w);
}
vec4 rotYZ(vec4 p, float a) {
    float c = cos(a), s = sin(a);
    return vec4(p.x, c*p.y - s*p.z, s*p.y + c*p.z, p.w);
}
vec4 rotYW(vec4 p, float a) {
    float c = cos(a), s = sin(a);
    return vec4(p.x, c*p.y - s*p.w, p.z, s*p.y + c*p.w);
}
vec4 rotZW(vec4 p, float a) {
    float c = cos(a), s = sin(a);
    return vec4(p.x, p.y, c*p.z - s*p.w, s*p.z + c*p.w);
}

// 4D box (hypercube) SDF: generalization of 3D box SDF to 4D
float sdBox4(vec4 p, vec4 b) {
    vec4 d = abs(p) - b;
    vec4 mx = max(d, vec4(0.0));
    float outside = length(mx);
    float inside = min(max(max(d.x, d.y), max(d.z, d.w)), 0.0);
    return outside + inside;
}

// NOWA FUNKCJA: Wyciąga lokalny, obrócony punkt w 4D
vec4 getLocalPoint(vec3 p3) {
    float wSlice = ubo.w_offset;
    vec4 p = vec4(p3, wSlice);

    float t = ubo.time;
    p = rotXY(p, ubo.aXY); 
    p = rotXZ(p, ubo.aXZ); 
    p = rotXW(p, ubo.aXW + t * 0.45); 
    p = rotYZ(p, ubo.aYZ); 
    p = rotYW(p, ubo.aYW); 
    p = rotZW(p, ubo.aZW);
    
    return p;
}

// mapScene korzysta teraz z getLocalPoint
float mapScene(vec3 p3) {
    vec4 p = getLocalPoint(p3);
    vec4 halfSize = vec4(0.9, 0.6, 0.4, 0.3);
    return sdBox4(p, halfSize);
}

// numeric normal computed by differentiating the slice SDF w.r.t x,y,z (w fixed)
vec3 getNormal(vec3 p) {
    float e = 1e-3;
    float dx = mapScene(p + vec3(e,0,0)) - mapScene(p - vec3(e,0,0));
    float dy = mapScene(p + vec3(0,e,0)) - mapScene(p - vec3(0,e,0));
    float dz = mapScene(p + vec3(0,0,e)) - mapScene(p - vec3(0,0,e));
    return normalize(vec3(dx, dy, dz));
}

vec3 rayDirFromUV(vec2 uv, float fov) {
    vec2 p = uv * 2.0 - 1.0;
    float z = 1.0 / tan(radians(fov) * 0.5);
    return normalize(vec3(p.x, p.y, -z));
}

float rayMarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    const int MAX_STEPS = 200;
    const float MAX_DIST = 80.0;
    const float EPS = 1e-3;
    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 pos = ro + rd * t;
        float d = mapScene(pos);
        if (d < EPS) return t;
        t += d;
        if (t > MAX_DIST) break;
    }
    return -1.0;
}

void main() {
    vec3 ro = vec3(0.0, 0.0, 4.0);
    vec3 rd = rayDirFromUV(uv, 45.0);

    float t = rayMarch(ro, rd);
    if (t > 0.0) {
        vec3 p = ro + rd * t;
        vec3 n = getNormal(p); // Wektor normalny do oświetlenia (zostaje w World-Space)

        // Pobieramy "lokalny" kształt punktu, żeby sprawdzić na której ścianie 4D wylądowaliśmy
        vec4 local_p = getLocalPoint(p);
        vec4 halfSize = vec4(0.9, 0.6, 0.4, 0.3);
        
        // Normalizujemy punkt względem rozmiaru sześcianu
        vec4 normalized_p = local_p / halfSize;
        vec4 abs_p = abs(normalized_p);

        vec3 faceColor = vec3(0.7);

        // Identyfikujemy ścianę (ta oś, która jest najbliżej 1.0 to nasza ściana)
        if (abs_p.x > abs_p.y && abs_p.x > abs_p.z && abs_p.x > abs_p.w) {
            faceColor = (local_p.x > 0.0) ? vec3(1.0, 0.3, 0.3) : vec3(0.6, 0.1, 0.1); // Osie X (Czerwone)
        } else if (abs_p.y > abs_p.x && abs_p.y > abs_p.z && abs_p.y > abs_p.w) {
            faceColor = (local_p.y > 0.0) ? vec3(0.3, 1.0, 0.3) : vec3(0.1, 0.6, 0.1); // Osie Y (Zielone)
        } else if (abs_p.z > abs_p.x && abs_p.z > abs_p.y && abs_p.z > abs_p.w) {
            faceColor = (local_p.z > 0.0) ? vec3(0.3, 0.5, 1.0) : vec3(0.1, 0.2, 0.7); // Osie Z (Niebieskie)
        } else {
            faceColor = (local_p.w > 0.0) ? vec3(1.0, 0.95, 0.2) : vec3(0.8, 0.5, 0.1); // Osie W 4-wymiaru (Żółte/Pomarańczowe)
        }

        // Oświetlenie kierunkowe (działa poprawnie, bo n jest w world-space!)
        vec3 lightDir = normalize(vec3(0.5, 0.7, -0.2));
        float diff = max(dot(n, lightDir), 0.0);
        vec3 col = faceColor * (0.25 + 0.75 * diff);

        outColor = vec4(col, 1.0);
    } else {
        outColor = vec4(0.01, 0.01, 0.02, 1.0);
    }
}
