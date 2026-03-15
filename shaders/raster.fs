#version 460 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;
uniform bool b_texture_diffuse1;
uniform vec3 color_diffuse;

uniform vec3 camera_pos;

const vec3 lightDirection = vec3(0.5, 1.0, 0.5);

// SDF DEBUG
uniform bool sdfDebug;
uniform sampler3D sdf;
uniform int textureSize;

uniform int debugAxis;
uniform float debugHeight;
uniform float debugLinesInterval;

void raster()
{
    vec3 N = normalize(normal);
    vec3 L = normalize(lightDirection);
    vec3 V = normalize(camera_pos - FragPos);

    float ambient = 0.1;

    float diffuse = max(dot(N, L), 0.0);

    float specular = 0.0;
    if (diffuse > 0.0)
    {
        vec3 R = reflect(-L, N);
        specular = pow(max(dot(V, R), 0.0), 32.0);
    }

    vec3 lighting = (ambient + diffuse + specular) * vec3(1.0);

    FragColor = vec4(lighting, 1.0);
}

void main()
{   
    FragColor = vec4(1.0, 0.0, 1.0, 1.0); // debug pink


    raster();
}