#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in float alphaCutoff;
layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec4 _col = texture(sampler2D(colorI, colorS), inUV);
	_col *= materialData.colorFactors;

	if (_col.w < alphaCutoff) {
		discard;
	}	

	// light calculations done in world space (normal interpolated in WS)
	float lightValue = max(dot(normalize(inNormal), sceneData.sunlightDirection.xyz), 0.1f);
	vec3 color = inColor * _col.xyz;
	//vec3 metal = texture(sampler2D(metalI, metalS), inUV).xyz;

	vec3 ambient = color *  sceneData.ambientColor.xyz;


	outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient , _col.w);
} 
