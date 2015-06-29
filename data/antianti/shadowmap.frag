#version 140

uniform bool linearizedShadowMap;

uniform vec2 lightZRange;

out vec4 depth;

bool transparency_discard();
float linearize(float depth, vec2 zRange);

void main()
{
    if (transparency_discard())
        discard;
        
    if (linearizedShadowMap)
        depth = vec4(linearize(gl_FragCoord.z, lightZRange));
    else
        depth = vec4(gl_FragCoord.z);
}
