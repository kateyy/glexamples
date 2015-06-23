#version 140

in vec3 a_vertex;
in vec3 a_normal;

out vec3 v_normal;
flat out int v_rand;

out vec4 v_shadowCoord;

uniform mat4 viewMatrix;
uniform mat4 projection;
uniform vec2 subpixelShift;  // [-0.5,0.5]
uniform float focalPlane;
uniform vec2 shearingFactor;
uniform mat4 biasedDepthTransform;

void main()
{
    vec4 posView = viewMatrix * vec4(a_vertex, 1.0);
    posView.xy += shearingFactor * (posView.z + focalPlane);
    
    vec4 pos = projection * posView;
    pos.xy /= pos.w;
    pos.xy += subpixelShift * 2.0;
    pos.xy *= pos.w;
    
	gl_Position = pos;
    v_normal = a_normal;
    v_rand = gl_VertexID;
    v_shadowCoord = biasedDepthTransform * vec4(a_vertex, 1.0);
}
