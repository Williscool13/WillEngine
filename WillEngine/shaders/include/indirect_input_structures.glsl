struct Vertex
{
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
	uint material_index;
	
};

struct Material
{
	vec4 color_factor;
	vec4 metal_rough_factors;
	uint texture_index1; // base color texture
	uint texture_index2; // metallic roughness texture
	uint sampler_index1; // base color sampler
	uint sampler_index2; // metallic roughness sampler
	float alpha_cutoff;
};

struct Model
{
	mat4 model;
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

layout(set = 1, binding = 0) uniform GlobalUniform
{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	uint model_count;
} sceneData;