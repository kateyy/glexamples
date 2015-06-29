#version 140

uniform sampler1D transparencyNoise1D;
uniform int transparencyNoise1DSamples;
uniform int frame;
uniform float transparency;
uniform vec2 viewport;



bool transparency_discard(int vertexID)
{
    int fragOffset = int(gl_FragCoord.x + gl_FragCoord.y * viewport.x);
    
    float random = texelFetch(transparencyNoise1D, 
        (frame + vertexID + fragOffset ) % int(transparencyNoise1DSamples), 0).x;
    // vec2 normFragCoord = gl_FragCoord.xy / viewport * vertexID * (float(time % 10240u) / 10240.0);
    return random < transparency;
}
