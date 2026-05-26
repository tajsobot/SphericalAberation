#version 330 core

layout(location = 0) in vec3 aPos; // Vertex position

void main()
{
    gl_Position = vec4(aPos, 0.5); // Pass through the vertex positions
}
