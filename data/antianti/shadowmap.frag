#version 140

uniform bool linearizedShadowMap;

out float depth;

bool transparency_discard();
float linearize(float depth);

void main()
{
    if (transparency_discard())
        discard;
        
    if (linearizedShadowMap)
        depth = linearize(gl_FragCoord.z);
    else
        depth = gl_FragCoord.z;

    gl_FragDepth = depth;
}
