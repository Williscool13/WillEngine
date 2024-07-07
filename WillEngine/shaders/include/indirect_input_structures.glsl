struct Vertex
{
	vec3 position;
	//float uv_x;
	float pad;
	vec3 normal;
	//float uv_y;
	float pad2;
	vec4 color;
	vec2 uv;
	uint material_index; // only use X, can pack with other values in future
	float pad3;
};

struct Material
{
	vec4 color_factor;
	vec4 metal_rough_factors;
	vec4 texture_image_indices;
	vec4 texture_sampler_indices;
	vec4 alpha_cutoff; // only use X, can pack with other values in future
};

struct Model
{
	mat4 model;
	uint vertex_offset;
	uint index_count;
	uint mesh_index;
	float pad;
};


layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer ModelData
{
	Model models[];
};

layout(buffer_reference, std430) readonly buffer MaterialData
{
	Material materials[];
};

layout(set = 0, binding = 0) uniform addresses
{
	VertexBuffer vertexbufferDeviceAddress;
	MaterialData materialBufferDeviceAddress;
	ModelData modelBufferDeviceAddress;
} buffer_addresses;

layout(set = 1, binding = 0) uniform sampler samplers[32];
layout(set = 1, binding = 1) uniform texture2D textures[255];

layout(set = 2, binding = 0) uniform GlobalUniform
{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 cameraPos;
} sceneData;


const float MAX_REFLECTION_LOD = 4.0;
const uint DIFF_IRRA_MIP_LEVEL = 5;
const bool FLIP_ENVIRONMENT_MAP_Y = true;


layout(set = 3, binding = 0) uniform samplerCube environmentDiffuseAndSpecular;
layout(set = 3, binding = 1) uniform sampler2D lut;

vec3 DiffuseIrradiance(vec3 N)
{
	vec3 ENV_N = N;
	if (FLIP_ENVIRONMENT_MAP_Y) { ENV_N.y = -ENV_N.y; }
	return textureLod(environmentDiffuseAndSpecular, ENV_N, DIFF_IRRA_MIP_LEVEL).rgb;
}

vec3 SpecularReflection(vec3 V, vec3 N, float roughness, vec3 F) {
	vec3 R = reflect(-V, N);
	if (FLIP_ENVIRONMENT_MAP_Y) { R.y = -R.y; }
	// dont need to skip mip 5 because never goes above 4
	vec3 prefilteredColor = textureLod(environmentDiffuseAndSpecular, R, roughness * MAX_REFLECTION_LOD).rgb;

	vec2 envBRDF = texture(lut, vec2(max(dot(N, V), 0.0f), roughness)).rg;

	return prefilteredColor * (F * envBRDF.x + envBRDF.y);
}