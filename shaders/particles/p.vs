#version 460 core

struct particle {
    vec3 position;
    vec3 speed;
    float lastTouch;
};

out float lastTouch;

uniform mat4 projection;
uniform mat4 view;

layout(std430, binding = 0) buffer ParticleBuffer
{
    particle particles[];
};

void main()
{
    particle p = particles[gl_VertexID];
    lastTouch = p.lastTouch;
    gl_Position = projection * view * vec4(p.position.xyz,1);
}