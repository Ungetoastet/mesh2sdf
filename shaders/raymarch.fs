#version 460 core

out vec4 frag_color;

// LIGHTING CONSTANTS
const vec3 lightDirection = normalize(vec3(0.5, 1.0, 0.5));

// RENDER CONSTANTS
const vec3 bg_color = vec3(0.1, 0.2, 0.2);
const int MAX_STEPS = 50;
const float MIN_DIST = 0.01;
const float MAX_DIST = 100;

// TEXTURE DATA
uniform vec3 color_diffuse;

// CAMERA DATA
uniform vec2 screen_resolution;
uniform mat4 inverse_view;
uniform vec3 camera_pos;

// MESH DATA
layout(std430, binding=0) buffer VertexBuffer {
    vec4 vertices[];
};
layout(std430, binding=1) buffer TopologyBuffer {
    uint topology[];
};

// STRUCT DEFINITIONS
struct ray{
    vec3 origin, direction;
};

struct face{
    vec3 a, b, c;
};

// HELPER FUNCTIONS
vec3 closestPointOnTriangle(vec3 p, vec3 a, vec3 b, vec3 c) {
    // From Real-Time Collision Detection (Christer Ericson)
    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ap = p - a;

    float d1 = dot(ab, ap);
    float d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) return a; // barycentric (1,0,0)

    // Check vertex B
    vec3 bp = p - b;
    float d3 = dot(ab, bp);
    float d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) return b; // barycentric (0,1,0)

    // Edge AB
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    // Check vertex C
    vec3 cp = p - c;
    float d5 = dot(ab, cp);
    float d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) return c; // barycentric (0,0,1)

    // Edge AC
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    // Edge BC
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    // Inside face region — use barycentric coordinates to project
    float denom = 1.0 / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

vec3 getFaceNormal(in face f) {
    return normalize(cross(f.b - f.a, f.c - f.a));
}

float distanceToFace(in vec3 point, in face f, out vec3 faceNormal) {
    faceNormal = getFaceNormal(f);
    vec3 cp = closestPointOnTriangle(point, f.a, f.b, f.c);
    return length(point - cp);
}

vec3 calcRayDirection(in vec2 fragCoord) {
    vec2 uv = fragCoord / screen_resolution;
    // simple pinhole: map to [-1,1]
    vec2 xy = (uv * 2.0 - 1.0) * vec2(screen_resolution.x / screen_resolution.y, 1.0);
    vec3 rayVS = normalize(vec3(xy, -(1.0 / tan(radians(45.0) / 2.0)))); // view-space
    vec3 rayWS = normalize((inverse_view * vec4(rayVS, 0.0)).xyz);
    return rayWS;
}

float sdf_naive(in vec3 pos, out vec3 normal) {
    float minDist = 1e10;
    vec3 closestNormal = vec3(0.0);

    for (uint i = 0; i < topology.length(); i+=3) {
        uint ia = topology[i + 0u];
        uint ib = topology[i + 1u];
        uint ic = topology[i + 2u];

        face f;
        f.a = (vertices[ia]).xyz;
        f.b = (vertices[ib]).xyz;
        f.c = (vertices[ic]).xyz;

        vec3 faceNormal;
        float d = distanceToFace(pos, f, faceNormal);
        if (d < minDist) {
            minDist = d;
            closestNormal = faceNormal;
        }
    }
    normal = closestNormal;
    return minDist;
}

bool traceRay(in ray r, out vec3 hitPos, out vec3 normal) {
    float t = 0.0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        hitPos = r.origin + r.direction * t;
        float dist = sdf_naive(hitPos, normal);
        t += dist;
        if (dist < MIN_DIST) {
            return true;
        }
        if (t > MAX_DIST) {
            return false;
        }
    }
    return false;
}

// MAIN PER PIXEL LOOP
void main() {
    vec2 frag = gl_FragCoord.xy;

    ray r;
    r.origin = camera_pos;
    r.direction = calcRayDirection(frag);

    vec3 hitPos;
    vec3 normal;

    if (traceRay(r, hitPos, normal)) {
        vec3 tempColor = color_diffuse;

        float diffuse = max(dot(normal, lightDirection), 0.0);
        vec3 finalColor = (0.2 + diffuse) * tempColor;
        frag_color = vec4(finalColor, 1.0);
    } 
    else {
        frag_color = vec4(bg_color, 1.0);
    }
}