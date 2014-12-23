#version 330

uniform mat4 projMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

layout (location = 0) in vec3 position;
layout (location = 2) in vec2 texCoord;

out vec2 TexCoord;

void main()
{
	TexCoord = vec2(texCoord);
	gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(position,1.0);
}