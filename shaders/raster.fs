#version 460 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;
uniform bool b_texture_diffuse1;
uniform vec3 color_diffuse;

uniform float alpha;

uniform vec3 camera_pos;

const vec3 lightDirection = vec3(0.5, 1.0, 0.5);

// SDF DEBUG
uniform bool sdfDebug;
uniform sampler3D sdf;
uniform int textureSize;

uniform int debugAxis;
uniform float debugHeight;
uniform float debugLinesInterval;

vec3 getDebugColor(float dist, int faceID) {
    if (faceID == -1) {
        return vec3(1.0, 0.0, 1.0);
    }
    if (dist >= 1e10) {
        return vec3(0.0, 1.0, 0.0);
    }
    else if (dist > 0.0) {
        float t = clamp(dist, 0.0, 10/debugLinesInterval);
        return mix(vec3(1.0), vec3(1.0, fract(dist*debugLinesInterval), 0.0), t);
    } 
    else {
        float t = clamp(-dist, 0.0, 10/debugLinesInterval);
        return mix(vec3(1.0), vec3(0.0, fract(dist*debugLinesInterval), 1.0), t);
    }
}


void raster()
{
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightDirection);
    vec3 viewDir = normalize(camera_pos - FragPos);

    float ambient = 0.1;

    float diffuse = max(dot(norm, lightDir), 0.0);

    vec3 reflectDir = reflect(-lightDir, norm);
    float specular = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 lighting = (ambient + diffuse + specular) * vec3(1.0);

    FragColor = vec4(lighting, alpha);
}

void debug()
{
    vec3 sampleCoords;

    if (debugAxis == 0) {
        // X slice → spans YZ (UVs swapped)
        sampleCoords = vec3(
            debugHeight,
            1.0-TexCoords.y,
            1.0-TexCoords.x
        );
    }
    else if (debugAxis == 1) {
        // Y slice → spans XZ (UVs OK)
        sampleCoords = vec3(
            TexCoords.x,
            debugHeight,
            TexCoords.y
        );
    }
    else {
        // Z slice → spans XY (UVs swapped)
        sampleCoords = vec3(
            1.0-TexCoords.y,
            1.0-TexCoords.x,
            debugHeight
        );
    }

    // if (fract(TexCoords.x * textureSize) < 0.05 || fract(TexCoords.y * textureSize) < 0.05)
        // discard;
        
    float dist = texture(sdf, sampleCoords).r;
    int faceID = int(texture(sdf, sampleCoords).a);
    vec3 debugCol = getDebugColor(dist, faceID);
    FragColor = vec4(debugCol, alpha);
}


void main()
{   
    FragColor = vec4(1.0, 0.0, 1.0, 1.0); // debug pink

    if (sdfDebug) {
        debug();
    } else {
        raster();
    } 
}