#version 140

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_texCoord;

flat out int v_vertexID;

uniform mat4 viewMatrix;
uniform mat4 projection;
uniform vec2 subpixelShift;  // [-0.5,0.5]
uniform float focalPlane;
uniform vec2 shearingFactor;
uniform mat4 biasedDepthTransform;

uniform vec3 light;

out vec3 v_worldPos;
out vec3 v_N;
out vec3 v_L;
out vec3 v_E;
out vec2 v_T;

out vec4 v_S;

void main()
{
    vec4 posView = viewMatrix * vec4(a_vertex, 1.0);
    posView.xy += shearingFactor * (posView.z + focalPlane);
    
    vec4 pos = projection * posView;
    pos.xy /= pos.w;
    pos.xy += subpixelShift * 2.0;
    pos.xy *= pos.w;
    
	gl_Position = pos;
    v_vertexID = gl_VertexID;

    vec4 lightPos = vec4(light, 1.0);
    v_N = a_normal;
    v_L = lightPos.xyz - posView.xyz;
    v_E = -posView.xyz;
    v_T = a_texCoord;

    v_worldPos = a_vertex;
    v_S = biasedDepthTransform * vec4(a_vertex, 1.0);
}
