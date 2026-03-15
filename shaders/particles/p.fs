#version 460 core

out vec4 FragColor;
in float lastTouch;

void main()
{
    float highlight = 1.0-(lastTouch/2.0);
    FragColor = vec4(highlight, 0.0, 1.0, clamp(highlight, 0.05, 1.0));
}