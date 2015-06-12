#version 140

uniform uint transparency;

out float fragTransparency;

void main()
{
    fragTransparency = float(transparency) / 255.0;
}
