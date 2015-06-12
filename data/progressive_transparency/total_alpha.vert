#version 140

in vec3 a_vertex;

uniform mat4 transform;


void main()
{
    gl_Position = transform * vec4(a_vertex, 1.0);
}
