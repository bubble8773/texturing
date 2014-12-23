#version 330

layout (location = 0) in vec4 position;

smooth out vec2 texCoord;
void main()
{
	// Positions are (-1,-1) to (+1,+1) in xy compoent. We convert them to (0,0) to (1,1) and assign them to texCoord
	texCoord = 0.5f*position.xy+vec2(0.5f,0.5f); 
	gl_Position = position;
}
