#version 430 core

uniform mat4 MVP;

layout (location = 0) in vec4 VertexPosition;
out vec3 Color;

void main(void)
{
	Color = vec3(clamp(VertexPosition.w / 10000,0,1),0,0);
	gl_Position = MVP * vec4(VertexPosition.x, VertexPosition.y, VertexPosition.z, 1);
}