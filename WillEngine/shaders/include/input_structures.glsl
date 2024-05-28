layout(set = 0, binding = 0) uniform  SceneData{

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 2, binding = 0) uniform GLTFMaterialData{

	vec4 colorFactors;
	vec4 metal_rough_factors;

} materialData;

layout(set = 1, binding = 0) uniform sampler colorSampler;
layout(set = 1, binding = 1) uniform texture2D colorTexture;
layout(set = 1, binding = 2) uniform sampler metalRoughSampler;
layout(set = 1, binding = 3) uniform texture2D metalRoughTexture;
