#version 140

in vec3 v_normal;
flat in int v_rand;

out vec4 fragColor;
out vec4 fragNormal;

uniform float transparency;
uniform vec2 viewport;
uniform uint time;
uniform int frame;

uniform sampler1D transparencyNoise1D;
uniform uint transparencyNoise1DSamples;


float rand();
float calculateAlpha(uint mask);

void main()
{
    // if ((v_rand % int(transparencyNoise1DSamples)) == 0)
    // {
        // fragColor = vec4(0.0);
        // fragNormal = vec4(v_normal, 1.0);
        // return;
    // }

    float random = rand();
    
    // fragColor = vec4(vec3(random), 1.0);
    // fragNormal = vec4(v_normal, 1.0);
    // return;
    
    if (random < transparency)
        discard;

    vec3 color = vec3(v_normal * 0.5 + 0.5);
    fragColor = vec4(color, 1.0);
    fragNormal = vec4(v_normal, 1.0);
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
    int fragOfffset = int(gl_FragCoord.x + gl_FragCoord.y * viewport.y);
    
    return texelFetch(transparencyNoise1D, 
        (frame + v_rand ) % int(transparencyNoise1DSamples), 0).x;
    // return texelFetch(transparencyNoise1D, frame % 1024, 0).x;
    // return texture(transparencyNoise1D, float(frame) / float(transparencyNoise1DSamples)).x;
    // return float(frame) / float(transparencyNoise1DSamples);
    // vec2 normFragCoord = gl_FragCoord.xy / viewport * v_rand * (float(time % 10240u) / 10240.0);
    // return rand(normFragCoord.xy);
}
