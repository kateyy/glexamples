#version 140

in vec2 v_uv;

uniform int frame;

uniform sampler2D ColorTexture;
uniform sampler2D lastFrame;

out vec4 outColor;


void main()
{
    vec4 color = texture(ColorTexture, v_uv);
    // outColor = color;
    vec4 last = texture(lastFrame, v_uv);
    outColor = mix(last, color, 1.0/float(frame));
}
