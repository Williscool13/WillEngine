layout(set = 0, binding = 0) uniform  SceneData{

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

//layout(set = 1, binding = 0) uniform sampler colorS;
//layout(set = 1, binding = 1) uniform sampler metalS;
//layout(set = 1, binding = 2) uniform texture2D colorI;
//layout(set = 1, binding = 3) uniform texture2D metalI;
layout(set = 1, binding = 0) uniform sampler samplers[2];
layout(set = 1, binding = 2) uniform texture2D textures[2];


layout(set = 2, binding = 0) uniform GLTFMaterialData{

	vec4 colorFactors;
	vec4 metal_rough_factors;

} materialData;
