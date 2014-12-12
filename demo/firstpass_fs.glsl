#version 430 core

struct Material
{
    vec3 colour;
    float shininess;
};

// bound and buffered on program start. only ever updated on program start (eg. never updates)
layout(std140, binding = 1) buffer BufferMaterials
{
    Material materials[];
};

in vec3 vs_pos;
in vec3 vs_normal;
flat in int vs_matIndex;

out vec4 position;
out vec4 normal;
out vec4 material;

void main(void)
{
    position = vec4(vs_pos, 1.0);
    normal = vec4(vs_normal, 1.0);
    material = vec4(materials[vs_matIndex].colour, materials[vs_matIndex].shininess);
}
