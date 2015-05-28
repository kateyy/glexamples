#version 150 core
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec3 a_vertex;
layout(location = 1) in vec3 a_normal;

out vec3 v_normal;

uniform mat4 transform;
uniform vec2 subpixelShift;

void main()
{
    vec4 pos = transform * vec4(a_vertex, 1.0);
    pos /= pos.w;
    pos.xy += subpixelShift;
	gl_Position = pos;
    v_normal = a_normal;
}
