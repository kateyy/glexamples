#version 140

in vec3 a_vertex;
in vec3 a_normal;

out vec3 v_normal;
flat out float v_rand;

uniform mat4 transform;


void main()
{
    gl_Position = transform * vec4(a_vertex, 1.0);
    v_normal = a_normal;
    v_rand = gl_VertexID;
}
