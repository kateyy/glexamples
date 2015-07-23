#version 150

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_texCoord;

flat out int v_vertexID;

uniform mat4 viewMatrix;
uniform mat4 projection;
uniform vec2 subpixelShift;  // [-0.5,0.5]
uniform float focalPlane;
uniform vec2 shearingFactor;

const uint numLights = 3u;
uniform mat4 biasedDepthTransforms[numLights];
uniform vec3 lightPositions[numLights];

out vec3 v_worldPos;
out vec3 v_N;
out vec3 v_L;
out vec3 v_E;
out vec2 v_T;

// TODO GLSL does not support array of arrays (geometry shader input)
// out vec4 v_S[numLights];

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

    vec4 lightPos = vec4(lightPositions[0], 1.0);   // TODO using first light only
    v_N = a_normal;
    v_L = lightPos.xyz - a_vertex.xyz;
    v_E = -posView.xyz;
    v_T = a_texCoord;

    v_worldPos = a_vertex;
    
    // for (uint i = 0; i < numLights; ++i)
        // v_S[i] = biasedDepthTransforms[i] * vec4(a_vertex, 1.0);
}
