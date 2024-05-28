#version 450

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
//output write
layout (location = 0) out vec4 outFragColor;


//texture to access
layout(set =0, binding = 0) uniform sampler sampler1;
layout(set =0, binding = 1) uniform texture2D texture1;


void main() 
{
	//return red
	//outFragColor = vec4(inColor,1.0f);
	
	//outFragColor = texture(displayTexture2,inUV);
	outFragColor = texture(sampler2D(texture1,sampler1),inUV);

}
