// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once
#include "will_engine.h"


class DescriptorBufferUniform;
class DescriptorBufferSampler;
class ShaderObject;

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct Vertex {

	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

//  Vertex Data
struct MultiDrawVertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 color;
	glm::vec2 uv;
	uint32_t materialIndex; // vertex is implicitly associated with a mesh, which is directly associated with a single material
};

struct MeshData {
	std::vector<MultiDrawVertex> vertices;
	std::vector<uint32_t> indices;
	size_t vertex_buffer_offset = 0;
	uint32_t index_buffer_offset = 0;
};



struct InstanceData {
	glm::mat4x4 modelMatrix; // will be accessed in shader through appropriate gl_instanceID
};


struct MaterialData {
	glm::vec4 color_factor;
	glm::vec4 metal_rough_factors;
	glm::vec4 texture_image_indices;
	glm::vec4 texture_sampler_indices;
	glm::vec4 alphaCutoff;
};

struct BoundindSphere {
	float x;
	float y;
	float z;
	float radius;
};


// holds the resources needed for a mesh
struct GPUMeshBuffers {

	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
	glm::mat4 modelMatrix;
	glm::mat3x4 invTransposeModelMatrix; // hard coded to 3x4 to match GLSL std140 layout
	VkDeviceAddress vertexBuffer;
	float alphaCutoff;
}; 


// Material Structure
enum class MaterialPass :uint8_t {
	MainColor,
	Transparent,
	Other
};
struct MaterialPipeline {
	//VkPipeline pipeline;
	std::shared_ptr<ShaderObject> shaderObject;
	VkPipelineLayout layout;
	DescriptorBufferSampler* materialTextureDescriptorBuffer;
	DescriptorBufferUniform* materialUniformDescriptorBuffer;
	VkDescriptorSetLayout materialTextureLayout;
	VkDescriptorSetLayout materialUniformLayout;
};

struct MaterialInstance {
	MaterialPipeline* pipeline;
	// descriptor properties
	VkDescriptorImageInfo colorDescriptorImageInfo;
	VkDescriptorImageInfo metalRoughDescriptorImageInfo;
	AllocatedBuffer materialUniformBuffer; // underlying buffer for the uniform descriptor

	int textureDescriptorBufferIndex;
	int uniformDescriptorBufferIndex;
	MaterialPass passType;
	float alphaCutoff;
};


struct DrawContext;

// base class for a renderable dynamic object
class IRenderable {

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

	// parent pointer must be a weak pointer to avoid circular dependencies
	std::weak_ptr<Node> parent;
	std::vector<std::shared_ptr<Node>> children;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	void refreshTransform(const glm::mat4& parentMatrix)
	{
		worldTransform = parentMatrix * localTransform;
		for (auto c : children) {
			c->refreshTransform(worldTransform);
		}
	}

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
	{
		// draw children
		for (auto& c : children) {
			c->Draw(topMatrix, ctx);
		}
	}
};



#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::print("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
