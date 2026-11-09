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

// Wyciąga lokalny, obrócony punkt w 4D.
vec4 getLocalPoint(vec3 p3) {
    float wSlice = ubo.w_offset;
    vec4 p = vec4(p3, wSlice);
    p.xyz -= vec3(0.0, 0.0, -2.5);

    p = rotXY(p, ubo.aXY);
    p = rotXZ(p, ubo.aXZ);
    p = rotXW(p, ubo.aXW);
    p = rotYZ(p, ubo.aYZ);
    p = rotYW(p, ubo.aYW);
    p = rotZW(p, ubo.aZW);
    
    return p;
}

float mapScene(vec3 p3) {
    float wSlice = ubo.w_offset;
    vec4 q = vec4(p3, wSlice);
    q.xyz -= vec3(0.0, 0.0, -2.5);

    q = rotXY(q, ubo.aXY);
    q = rotXZ(q, ubo.aXZ);
    q = rotXW(q, ubo.aXW);
    q = rotYZ(q, ubo.aYZ);
    q = rotYW(q, ubo.aYW);
    q = rotZW(q, ubo.aZW);

    vec4 halfSize = vec4(0.2);
    return sdBox4(q, halfSize);
}

// numeric normal computed by differentiating the slice SDF w.r.t x,y,z (w fixed)
vec3 getNormal(vec3 p) {
    float e = 1e-3;
    float dx = mapScene(p + vec3(e,0,0)) - mapScene(p - vec3(e,0,0));
    float dy = mapScene(p + vec3(0,e,0)) - mapScene(p - vec3(0,e,0));
    float dz = mapScene(p + vec3(0,0,e)) - mapScene(p - vec3(0,0,e));
    return normalize(vec3(dx, dy, dz));
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

vec3 getFaceColor(vec4 localPoint, vec4 halfSize) {
    vec4 faceCoordinate = abs(localPoint / halfSize);

    if (faceCoordinate.x >= faceCoordinate.y &&
        faceCoordinate.x >= faceCoordinate.z &&
        faceCoordinate.x >= faceCoordinate.w) {
        return localPoint.x >= 0.0 ? vec3(1.0, 0.15, 0.15) : vec3(0.55, 0.02, 0.02);
    }
    if (faceCoordinate.y >= faceCoordinate.z &&
        faceCoordinate.y >= faceCoordinate.w) {
        return localPoint.y >= 0.0 ? vec3(0.15, 1.0, 0.15) : vec3(0.02, 0.55, 0.02);
    }
    if (faceCoordinate.z >= faceCoordinate.w) {
        return localPoint.z >= 0.0 ? vec3(0.15, 0.35, 1.0) : vec3(0.02, 0.08, 0.55);
    }
    return localPoint.w >= 0.0 ? vec3(1.0, 0.85, 0.05) : vec3(0.85, 0.3, 0.02);
}

void main() {
    mat4 invView = inverse(ubo.view);
    mat4 invProj = inverse(ubo.proj);
    vec3 ro = invView[3].xyz;

    vec2 ndc = uv * 2.0 - 1.0;

    vec4 rayView = invProj * vec4(ndc, 1.0, 1.0);
    vec3 rayDirLocal = normalize(rayView.xyz / rayView.w);
    vec3 rd = normalize((invView * vec4(rayDirLocal, 0.0)).xyz);

    float t = rayMarch(ro, rd);
    if (t > 0.0) {
        vec3 p = ro + rd * t;
        // Pobieramy "lokalny" kształt punktu, żeby sprawdzić na której ścianie 4D wylądowaliśmy
        vec4 local_p = getLocalPoint(p);
        vec4 halfSize = vec4(0.2);
        
        vec3 faceColor = getFaceColor(local_p, halfSize);
        outColor = vec4(faceColor, 1.0);
    } else {
        outColor = vec4(0.01, 0.01, 0.02, 1.0);
    }
}
