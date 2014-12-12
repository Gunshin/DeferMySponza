#version 430

struct Light
{
    vec3 position;
    float range;
};

layout(std140, binding = 0) buffer BufferRender
{
    mat4 projectionViewMat;
    vec3 camPosition;
};

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec3 lightPosition;
layout (location = 3) in float lightRange;

out Light vs_light;

void main(void)
{
    Light light;
    light.position = lightPosition;
    light.range = lightRange;
	vs_light = light;

	gl_Position = projectionViewMat * vec4(vertexPosition, 1.0);
}
