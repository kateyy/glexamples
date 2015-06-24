#version 140

in vec3 a_vertex;
flat out int v_rand;

uniform mat4 transform;

void main()
{
	vec4 vertex = transform * vec4(a_vertex, 1.0);
    v_rand = gl_VertexID;
	gl_Position = vertex;
}
