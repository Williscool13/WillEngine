#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#include "indirect_input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) flat in uint inMaterialIndex;
layout (location = 0) out vec4 outFragColor;



layout(set = 2, binding = 0) uniform sampler samplers[32];
layout(set = 2, binding = 1) uniform texture2D textures[255];


void main() 
{
	Material m = buffer_addresses.materialBufferDeviceAddress.materials[inMaterialIndex];
	uint colorSamplerIndex = uint(m.texture_sampler_indices.x);
	uint colorImageIndex =	 uint(m.texture_image_indices.x);
	vec4 _col = texture(sampler2D(textures[colorImageIndex], samplers[colorSamplerIndex]), inUV);
	//vec4 _col = texture(sampler2D(colorI, colorS), inUV);
	//vec4 _col = vec4(texture(textures[m.texture_index1], inUV));
	
	//vec4 _col = vec4(inColor, 1.0);
	_col *= m.color_factor;
	//vec4 _col = m.color_factor;

	if (_col.w < m.alpha_cutoff.w) {
		discard;
	}

	// light calculations done in world space (normal interpolated in WS)
	float lightValue = max(dot(normalize(inNormal), sceneData.sunlightDirection.xyz), 0.1f);
	vec3 color = inColor * _col.xyz;
	//vec3 metal = texture(sampler2D(metalI, metalS), inUV).xyz;

	vec3 ambient = color *  sceneData.ambientColor.xyz;


	outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient , _col.w);
	//outFragColor = vec4(vec3(m.color_factor.xyz), 1.0f);
} 