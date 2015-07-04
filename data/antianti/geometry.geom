#version 150

layout(triangles) in;
layout (triangle_strip, max_vertices=6) out;

flat in int v_vertexID[3];
in vec3 v_worldPos[3];
in vec3 v_N[3];
in vec3 v_L[3];
in vec3 v_E[3];
in vec2 v_T[3];

in vec4 v_S[3];

flat out int g_vertexID;
out vec3 g_worldPos;
out vec3 g_N;
out vec3 g_N_face;
out vec3 g_L;
out vec3 g_E;
out vec2 g_T;

out vec4 g_S;

void main()
{

       vec3 normal = normalize(cross(v_worldPos[1] - v_worldPos[0], v_worldPos[2] -v_worldPos[0]));
       for(int i = 0; i < gl_in.length(); i++)
       {
            gl_Position = gl_in[i].gl_Position;

            g_vertexID = v_vertexID[i];
            g_worldPos = v_worldPos[i];
            g_N = v_N[i];
            g_N_face = normal;
            g_L = v_L[i];
            g_E = v_E[i];
            g_T = v_T[i];
            g_S = v_S[i];

            EmitVertex();
       }
// use this to fix normals in the jakobi szene
//       EndPrimitive();
//       normal = normalize(cross(v_worldPos[2] - v_worldPos[0], v_worldPos[1] -v_worldPos[0]));
//       for(int i = gl_in.length()-1; i >= 0; i--)
//       {
//            gl_Position = gl_in[i].gl_Position;
//
//            g_vertexID = v_vertexID[i];
//            g_worldPos = v_worldPos[i];
//            g_N = v_N[i];
//            g_N_face = normal;
//            g_L = v_L[i];
//            g_E = v_E[i];
//            g_T = v_T[i];
//            g_S = v_S[i];
//
//            EmitVertex();
//       }
//       EndPrimitive();
}