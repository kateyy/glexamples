#version 150 core
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec3 a_vertex;
layout(location = 1) in vec3 a_normal;

out vec3 v_normal;

uniform mat4 viewMatrix;
uniform mat4 projection;
uniform mat4 transform;
uniform vec2 subpixelShift;  // [-0.5,0.5]
uniform float focalPlane;
uniform vec2 shearingFactor;

void main()
{
    vec4 posView = viewMatrix * vec4(a_vertex, 1.0);
    posView.xy += shearingFactor * (posView.z + focalPlane);
    
    vec4 pos = projection * posView;
    pos /= pos.w;
    pos.xy += subpixelShift * 2.0;
    
	gl_Position = pos;
    v_normal = a_normal;
}
