#version 140

in vec3 v_normal;
flat in float v_rand;

out vec4 fragColor;

uniform float transparency;
uniform vec2 viewport;
uniform uint time;


float rand();
float calculateAlpha(uint mask);

void main()
{
    float random = rand();
    
    if (random < transparency)
        discard;

    vec3 color = vec3(v_normal * 0.5 + 0.5);
    fragColor = vec4(color, 1.0);
}

highp float rand(vec2 co)
{
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt= dot(co.xy ,vec2(a,b));
    highp float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}

float rand()
{
    vec2 normFragCoord = floor(gl_FragCoord.xy) / viewport * v_rand * float(time % 1024u) / 1024.0;
    return rand(normFragCoord.xy);
}
