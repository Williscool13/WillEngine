#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
//#include "indirect_input_structures.glsl"

//layout (location = 0) in vec3 inNormal;
//layout (location = 1) in vec3 inColor;
//layout (location = 2) in vec2 inUV;
//layout (location = 3) flat in uint inMaterialIndex;
layout (location = 0) out vec4 outFragColor;



//layout(binding = 2, set = 0) uniform sampler2D textures[225];


void main() 
{
	//Material m = buffer_addresses.materialBufferDeviceAddress.materials[inMaterialIndex];
	//vec4 _col = texture(sampler2D(colorI, colorS), inUV);
	//vec4 _col = vec4(texture(textures[m.texture_index1], inUV));
	//vec4 _col = vec4(inColor, 1.0);
	//_col *= m.color_factor;

	//if (_col.w < m.alpha_cutoff) {
	//		discard;
	//	}	

	// light calculations done in world space (normal interpolated in WS)
	//float lightValue = max(dot(normalize(inNormal), sceneData.sunlightDirection.xyz), 0.1f);
	//vec3 color = inColor * _col.xyz;
	//vec3 metal = texture(sampler2D(metalI, metalS), inUV).xyz;

	//vec3 ambient = color *  sceneData.ambientColor.xyz;


	//outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient , _col.w);
	outFragColor = vec4(1.0, 0.0, 0.0, 1.0);
} 
