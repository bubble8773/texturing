#version 330

uniform vec4 diffuseColor;
uniform int texCount;

uniform	sampler2D texUnit;

in vec2 TexCoord;

out vec4 output;

void main()
{
	vec4 color;
	if (texCount == 0) 
		color = diffuseColor;
	else 
		color = texture2D(texUnit, TexCoord);
	output = color;
}
