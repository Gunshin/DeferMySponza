#version 430

uniform sampler2DRect sampler_colour;

out vec4 out_colour;

float GetLuminanceValue(int x_, int y_);

/*

currently does nothing but preparation for any post process stuff in the future

*/
void main(void)
{
	ivec2 pixelCoord = ivec2(gl_FragCoord.xy);

    float pixelLuminance = GetLuminanceValue(pixelCoord.x, pixelCoord.y);
    float luminance[8];
    luminance[0] = GetLuminanceValue(pixelCoord.x - 1, pixelCoord.y);
    luminance[1] = GetLuminanceValue(pixelCoord.x + 1, pixelCoord.y);
    luminance[2] = GetLuminanceValue(pixelCoord.x, pixelCoord.y + 1);
    luminance[3] = GetLuminanceValue(pixelCoord.x, pixelCoord.y - 1);
    luminance[4] = GetLuminanceValue(pixelCoord.x - 1, pixelCoord.y - 1);
    luminance[5] = GetLuminanceValue(pixelCoord.x + 1, pixelCoord.y - 1);
    luminance[6] = GetLuminanceValue(pixelCoord.x + 1, pixelCoord.y + 1);
    luminance[7] = GetLuminanceValue(pixelCoord.x - 1, pixelCoord.y + 1);

    float average = 0;
    for(int i = 0; i < luminance.length(); ++i)
    {
        average += luminance[i];
    }

    average /= luminance.length();
    float lDiff = abs(pixelLuminance - average);

    if(lDiff > 0.01)
    {
        vec3 finalColour = vec3(0, 0, 0);
        for(int i = -1; i < 2; ++i)
        {
    
            for(int j = -1; j < 2; ++j)
            {
        
                finalColour += texelFetch(sampler_colour, ivec2(pixelCoord.x + i, pixelCoord.y + j)).rgb;

            }
    
        }

        finalColour /= 9;

        out_colour = vec4(finalColour, 1);
    }
    else
    {
	    out_colour = texelFetch(sampler_colour, pixelCoord);
    }

    //out_colour = lDiff > 0.01 ? vec4(1, 1, 1, 1) : vec4(0, 0, 0, 1);
}

float GetLuminanceValue(int x_, int y_)
{

    vec4 pixelCol = texelFetch(sampler_colour, ivec2(x_, y_));
    return (0.2126 * pixelCol.r) + (0.7152 * pixelCol.g) + (0.0722 * pixelCol.b);

}