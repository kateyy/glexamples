#version 140

uniform bool linearizedShadowMap;

out float depth;

float linearize(float depth);

void main()
{
    if (linearizedShadowMap)
        depth = linearize(gl_FragCoord.z);
    else
        depth = gl_FragCoord.z;

    gl_FragDepth = depth;
}
