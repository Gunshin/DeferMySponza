#version 430

uniform sampler2DRect sampler_colour;

out vec4 out_colour;

/*

currently does nothing but preparation for any post process stuff in the future

*/
void main(void)
{
	ivec2 pixelCoord = ivec2(gl_FragCoord.xy);

	out_colour = texelFetch(sampler_colour, pixelCoord).rgba;
}