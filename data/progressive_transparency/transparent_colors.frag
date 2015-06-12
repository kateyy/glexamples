#version 140

in vec3 v_normal;

out vec4 fragColor;

uniform uint transparency;


void main()
{
    float alpha = float(transparency) / 255.0;
    vec3 color = vec3(v_normal * 0.5 + 0.5);
    fragColor = vec4(color * alpha, alpha);
}
