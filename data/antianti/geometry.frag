#version 140

in vec3 v_normal;
in vec4 v_shadowCoord;

out vec4 fragColor;
out vec4 fragNormal;

uniform uint time;
uniform sampler2D shadowMap;
uniform bool linearizedShadowMap;


bool transparency_discard();
float calculateAlpha(uint mask);

// depth_util.frag
float linearize(float depth);

void main()
{
    if (transparency_discard())
        discard;

    vec3 shadowCoord = v_shadowCoord.xyz / v_shadowCoord.w;
    if (linearizedShadowMap)
        shadowCoord.z = linearize(shadowCoord.z);
    shadowCoord.z -= 0.0001;
    float shadowDist = texture(shadowMap, shadowCoord.xy).x;
    
    float shadow = step(0.0, sign(v_shadowCoord.w)) * step(shadowCoord.z, shadowDist);
    
	// vec3 color = vec3(shadow);

    vec3 color = shadow * vec3(v_normal * 0.5 + 0.5);
    fragColor = vec4(color, 1.0);
    fragNormal = vec4(v_normal, 1.0);
}
