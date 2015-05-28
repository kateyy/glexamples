#version 420

in vec2 v_uv;

uniform int frame;

layout(binding=0) uniform sampler2D ColorTexture;
layout(binding=1) uniform sampler2D lastFrame;

layout (location=0) out vec4 outColor;


void main()
{
    vec4 color = texture(ColorTexture, v_uv);
    // outColor = color;
    vec4 last = texture(lastFrame, v_uv);
    outColor = mix(last, color, 1.0/float(frame));
}
