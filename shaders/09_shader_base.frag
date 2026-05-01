#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push { float time; } pc;

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

// mapScene: takes 3D point p (the ray sample), forms a 4D point (p.x,p.y,p.z,wSlice),
// applies 4D rotations, returns distance to 4D box sliced at that w
float mapScene(vec3 p3) {
    // choose slice coordinate w (we keep it constant here; can be animated or controlled)
    float wSlice = 0.0; // change to e.g. 0.2 to move slice
    vec4 p = vec4(p3, wSlice);

    // build rotation angles from time (animated)
    float t = pc.time;
    float aXY = t * 0.6;
    float aXZ = t * 0.35;
    float aXW = t * 0.45;
    float aYZ = t * 0.25;
    float aYW = t * 0.5;
    float aZW = t * 0.15;

    // primary rotation: XW plane (single-angle rotation)
    p = rotXW(p, aXW);
    // optional extra rotations (uncomment to enable double/compound rotation)
    // p = rotXY(p, aXY);
    // p = rotXZ(p, aXZ);
    // p = rotYZ(p, aYZ);
    // p = rotYW(p, aYW);
    // p = rotZW(p, aZW);

    // hyperbox half-sizes in 4D
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
        vec3 n = getNormal(p);

        // color by normal for shape cues
        vec3 an = abs(n);
        vec3 faceColor = vec3(0.7);
        if (an.x > an.y && an.x > an.z) {
            faceColor = (n.x > 0.0) ? vec3(1.0, 0.3, 0.3) : vec3(0.6, 0.2, 0.6);
        } else if (an.y > an.x && an.y > an.z) {
            faceColor = (n.y > 0.0) ? vec3(0.3, 1.0, 0.3) : vec3(0.2, 0.7, 0.7);
        } else {
            faceColor = (n.z > 0.0) ? vec3(0.3, 0.5, 1.0) : vec3(1.0, 0.95, 0.2);
        }

        vec3 lightDir = normalize(vec3(0.5, 0.7, -0.2));
        float diff = max(dot(n, lightDir), 0.0);
        vec3 col = faceColor * (0.25 + 0.75 * diff);

        outColor = vec4(col, 1.0);
    } else {
        outColor = vec4(0.01, 0.01, 0.02, 1.0);
    }
}
