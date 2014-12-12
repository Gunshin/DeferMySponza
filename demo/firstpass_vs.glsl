#version 430

layout(std140, binding = 0) buffer BufferRender
{
    mat4 projectionViewMat;
    vec3 camPosition;
};

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in mat4x3 instanceMat;
layout (location = 6) in int matIndex;

out vec3 vs_pos;
out vec3 vs_normal;

flat out int vs_matIndex;

void main(void)
{
	vs_matIndex = matIndex;
	vec4 pos = vec4(instanceMat * vec4(position, 1), 1);
	vs_pos = vec3(pos);
	vs_normal = normalize(mat3(instanceMat) * normal);

	gl_Position = projectionViewMat * pos;
}
