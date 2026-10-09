#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

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

// 4D box (hypercube) SDF
float sdBox4(vec4 p, vec4 b) {
    vec4 d = abs(p) - b;
    vec4 mx = max(d, vec4(0.0));
    float outside = length(mx);
    float inside = min(max(max(d.x, d.y), max(d.z, d.w)), 0.0);
    return outside + inside;
}

// 4D sphere SDF
float sdSphere4(vec4 p, float r) {
    return length(p) - r;
}

// Exact 4D capsule: distance to a segment in 4D, minus radius
float sdCapsule4(vec4 p, vec4 a, vec4 b, float r) {
    vec4 pa = p - a;
    vec4 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

// Rounded 4D box
float sdRoundedBox4(vec4 p, vec4 b, float r) {
    return sdBox4(p, b) - r;
}

// 4D ellipsoid-like shape
float sdEllipsoid4(vec4 p, vec4 r) {
    vec4 q = p / r;
    float scale = min(min(r.x, r.y), min(r.z, r.w));
    return (length(q) - 1.0) * scale;
}

// 4D "Clifford-torus-like" form:
// two 2D circles coupled together in orthogonal planes
float sdCliffordTorus4(vec4 p, vec2 majorRadii, float tubeR) {
    float a = length(p.xy) - majorRadii.x;
    float b = length(p.zw) - majorRadii.y;
    return length(vec2(a, b)) - tubeR;
}

// Simple 4D cross-polytope
float sdCrossPolytope4(vec4 p, float r) {
    return (abs(p.x) + abs(p.y) + abs(p.z) + abs(p.w)) - r;
}

// Stała pozycja tesseraktu w przestrzeni 3D
const vec3 TESSERACT_POS = vec3(0.0, 0.0, 0.0);

// NOWA FUNKCJA: Wyciąga lokalny, obrócony punkt w 4D
vec4 getLocalPoint(vec3 p3) {
    // Przesunięcie punktu ray-marching do lokalnego CS tesseraktu
    // Dzięki temu tesserakt pozostaje nieruchomy w przestrzeni, niezależnie od pozycji kamery
    vec3 p_local = p3 - TESSERACT_POS;
    
    float wSlice = ubo.w_offset;
    vec4 p = vec4(p_local, wSlice);

    float t = ubo.time;
    p = rotXY(p, ubo.aXY); 
    p = rotXZ(p, ubo.aXZ); 
    p = rotXW(p, ubo.aXW + t * 0.45); 
    p = rotYZ(p, ubo.aYZ); 
    p = rotYW(p, ubo.aYW); 
    p = rotZW(p, ubo.aZW);
    
    return p;
}

int shapeId = 0; // 0=box, 1=sphere, 2=capsule, 3=rounded box, 4=ellipsoid, 5=torus-like, 6=cross-polytope

float mapScene(vec3 p3) {
    vec4 p = getLocalPoint(p3);

    if (shapeId == 0) {
        return sdBox4(p, vec4(0.45, 0.30, 0.20, 0.15));
    } else if (shapeId == 1) {
        return sdSphere4(p, 0.45);
    } else if (shapeId == 2) {
        return sdCapsule4(
            p,
            vec4(-0.35, 0.0, 0.0, 0.0),
            vec4( 0.35, 0.0, 0.0, 0.0),
            0.15
        );
    } else if (shapeId == 3) {
        return sdRoundedBox4(p, vec4(0.35, 0.25, 0.18, 0.12), 0.06);
    } else if (shapeId == 4) {
        return sdEllipsoid4(p, vec4(0.50, 0.35, 0.25, 0.20));
    } else if (shapeId == 5) {
        return sdCliffordTorus4(p, vec2(0.35, 0.35), 0.10);
    }else {
        return sdCrossPolytope4(p, 0.75);
    }
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

float rayMarch_fast(vec3 ro, vec3 rd) {
    float t = 0.0;
    const int MAX_STEPS = 200;
    const float MAX_DIST = 50.0;
    const float EPS = 1e-3;
    const float MULT_START = 1.0;
    const float MULT_GROWTH = 1.1;

    float mult = MULT_START;

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 pos = ro + rd * t;
        float d = mapScene(pos);

        if (abs(d) < EPS) {
            return t;
        }

        float prevT = t;
        float stepDist = d * mult;
        t += stepDist;

        if (t > MAX_DIST) {
            return -1.0;
        }

        if (d < 0.0) {
            float a = prevT;
            float b = t;

            for (int j = 0; j < 8; ++j) {
                float m = 0.5 * (a + b);
                float dm = mapScene(ro + rd * m);

                if (abs(dm) < EPS) {
                    return m;
                }

                if (dm > 0.0) {
                    a = m;
                } else {
                    b = m;
                }
            }

            return 0.5 * (a + b);
        }

        if (mult < 1.7){
            mult *= MULT_GROWTH;
        }
    }

    return -1.0;
}

void main() {
    // Rotacja kamery
    mat4 invView = inverse(ubo.view);
    
    // Ray origin: zawsze w tym samym punkcie dla obu oczu (center point, nie IPD-shifted)
    // Ignorujemy translacyjny offset z IPD - pracujemy tylko z rotacją
    vec3 ro = vec3(0.0, 1.3, 3.0);  // Stała pozycja (jak w STAGE reference space)

    // Obliczenie wektora promienia z uwzględnieniem perspektywy i FOV gogli
    mat4 invProj = inverse(ubo.proj);
    vec4 target = invProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rayDirLocal = normalize(target.xyz / target.w);
    vec3 rd = normalize((invView * vec4(rayDirLocal, 0.0)).xyz);

    float t = rayMarch(ro, rd);
    
    if (t > 0.0) {
        vec3 p = ro + rd * t;
        vec3 n = getNormal(p);

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