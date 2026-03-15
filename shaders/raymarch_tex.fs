#version 460 core

out vec4 frag_color;

// MESH DATA
layout(std430, binding=0) buffer VertexBuffer {
    vec4 vertices[];
};
layout(std430, binding=1) buffer TopologyBuffer {
    uint topology[];
};

layout (binding = 3, rgba32f) uniform readonly image3D rawSDF;

// LIGHTING CONSTANTS
const vec3 lightDirection = normalize(vec3(0.5, 1.0, 0.5));

// RENDER CONSTANTS
const vec3 bg_color = vec3(0.2, 0.2, 0.2);
const int MAX_STEPS = 50;
const float MIN_DIST = 0.01;
const float MAX_DIST = 10000;

// TEXTURE DATA
uniform vec3 color_diffuse;

// CAMERA DATA
uniform vec2 screen_resolution;
uniform mat4 inverse_view;
uniform vec3 camera_pos;

// MESH DATA
uniform sampler3D sdf;
uniform mat2x3 boundingBox;
uniform float bounding_box_padding;
uniform int sdfSize;

// STRUCT DEFINITIONS
struct ray{
    vec3 origin, direction;
};

// HELPER FUNCTIONS

// Returns 0 if the point is inside the axis aligned bounding box. If not, returns the distance
float pointInBoundingBox(in vec3 p) {
    vec3 closestPointInBox = clamp(p, boundingBox[0], boundingBox[1]);
    return distance(p, closestPointInBox);
}

vec3 calcRayDirection(in vec2 fragCoord) {
    vec2 uv = fragCoord / screen_resolution;
    // simple pinhole: map to [-1,1]
    vec2 xy = (uv * 2.0 - 1.0) * vec2(screen_resolution.x / screen_resolution.y, 1.0);
    vec3 rayVS = normalize(vec3(xy, -(1.0 / tan(radians(45.0) / 2.0)))); // view-space
    vec3 rayWS = normalize((inverse_view * vec4(rayVS, 0.0)).xyz);
    return rayWS;
}

float sdf_get(in vec3 pos, out vec2 uv, out uint faceID) {
    float inBounds = pointInBoundingBox(pos);
    vec3 texCoords = (pos - boundingBox[0]) / (boundingBox[1] - boundingBox[0]);
    
    vec4 samp = texture(sdf, texCoords);
    float modelDist = samp.r;
    uv = samp.gb;
    faceID = uint(imageLoad(rawSDF, ivec3(texCoords * sdfSize)).a);
    
    return max(modelDist, inBounds);
}

vec3 getNormal(uint faceID) {
    uint base = faceID * 3u;

    uint ia = topology[base + 0u];
    uint ib = topology[base + 1u];
    uint ic = topology[base + 2u];

    vec3 a = vertices[ia].xyz;
    vec3 b = vertices[ib].xyz;
    vec3 c = vertices[ic].xyz;

    return normalize(cross(b - a, c - a));
}

bool traceRay(in ray r, out vec3 hitPos, out vec3 normal) {
    float t = 0.0;

    for (int i = 0; i < MAX_STEPS; ++i) {
        hitPos = r.origin + r.direction * t;
        uint faceID;
        vec2 uv;
        float dist = sdf_get(hitPos, uv, faceID);
        if (dist < MIN_DIST) {
            normal = getNormal(faceID);
            return true;
        }
        t += dist;
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
    vec3 norm;

    if (traceRay(r, hitPos, norm)) {
        vec3 tempColor = color_diffuse;

        vec3 lightDir = normalize(lightDirection);
        vec3 viewDir = normalize(camera_pos - hitPos);

        float ambient = 0.1;

        float diffuse = max(dot(norm, lightDir), 0.0);

        vec3 reflectDir = reflect(-lightDir, norm);
        float specular = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

        vec3 lighting = (ambient + diffuse + specular) * vec3(1.0);
        frag_color = vec4(lighting, 1.0);
    } 
    else {
        frag_color = vec4(bg_color, 1.0);
    }
}