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

vec3 calculateColour(vec3 lightPos_, float lightRange_, vec3 fragPos_, vec3 fragNorm_, vec3 V_, float shininess_);

uniform sampler2DRect sampler_world_position;
uniform sampler2DRect sampler_world_normal;
uniform sampler2DRect sampler_world_mat;

in Light vs_light;

out vec3 reflected_light;

void main(void)
{
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    vec3 position = texelFetch(sampler_world_position, pixelCoord).xyz;
    vec3 normal = texelFetch(sampler_world_normal, pixelCoord).xyz;
	vec4 matColour = texelFetch(sampler_world_mat, pixelCoord).rgba; // the alpha value is the shininess of the material

    vec3 V = normalize(camPosition - position);

    vec3 col = calculateColour(vs_light.position, vs_light.range, position, normal, V, matColour.a);

	reflected_light = col * matColour.rgb;
}

vec3 calculateColour(vec3 lightPos_, float lightRange_, vec3 fragPos_, vec3 fragNorm_, vec3 V_, float shininess_)
{
	
	vec3 L = normalize(lightPos_ - fragPos_);

	vec3 R = normalize(reflect(-L, fragNorm_));

	float distance = distance(fragPos_, lightPos_);

    vec3 attenuatedDistance = vec3(1.0, 1.0, 1.0) * smoothstep(lightRange_, 1, distance);

    vec3 attenuatedLight = attenuatedDistance;

	vec3 Id = max(dot(L, fragNorm_), 0) * attenuatedLight;

	vec3 Is = vec3(0, 0, 0);
	if (dot(L, fragNorm_) > 0 && shininess_ > 0)
	{
		Is = vec3(1, 1, 1) * pow(max(0, dot(R, V_)), shininess_) * attenuatedLight;
	}

	return Id + Is;
}