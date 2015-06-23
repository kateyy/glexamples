#version 140

in vec3 a_vertex;

uniform mat4 transform;

void main()
{
	vec4 vertex = transform * vec4(a_vertex, 1.0);
	gl_Position = vertex;
}
