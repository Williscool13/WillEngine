﻿// vulkan_guide.h : Include file for standard system include files,
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

//  Vertex Data
struct MultiDrawVertex {
	glm::vec3 position;
	float pad;
	glm::vec3 normal;
	float pad2;
	glm::vec4 color;
	glm::vec2 uv;
	uint32_t materialIndex; // vertex is implicitly associated with a mesh, which is directly associated with a single material
	float pad3;
};

// Raw Mesh Data
struct RawMeshData {
	std::vector<MultiDrawVertex> vertices;
	std::vector<uint32_t> indices;
	bool hasTransparent = false;
};

// Processed Mesh Data (not passed to GPU)
struct MeshData {
	std::vector<uint32_t> indices;
	uint32_t index_buffer_offset = 0;
	bool transparent = false; // true if any primitive in the mesh is transparent
};

// Per mesh instance data
struct InstanceData {
	glm::mat4x4 modelMatrix; // will be accessed in shader through appropriate gl_instanceID
	uint32_t vertexOffset; 
	// slight data redundancy here, since this value will be the same for each instance that uses the same mesh
	// significantly better than duplicating vertices in the vertex buffer
	uint32_t indexCount;
	uint32_t meshIndex;
	float padding;
};

// Per material data
struct MaterialData {
	glm::vec4 color_factor;
	glm::vec4 metal_rough_factors;
	glm::vec4 texture_image_indices;   // x: base color, y: metallic roughness, z: pad, w: pad
	glm::vec4 texture_sampler_indices; // x: base color, y: metallic roughness, z: pad, w: pad
	glm::vec4 alphaCutoff; // x: alpha cutoff, y: alpha mode, z: padding, w: padding
};

struct BoundingSphere {
	BoundingSphere() = default;
	explicit BoundingSphere(const RawMeshData& meshData);
	glm::vec3 center{};
	float radius{};
};

struct ComputeCullingData {
	VkDeviceAddress opaqueCommandBufferAddress;
	uint32_t opaqueCommandBufferCount;
	VkDeviceAddress transparentCommandBufferAddress;
	uint32_t transparentCommandBufferCount;
	VkDeviceAddress meshBoundsAddress;
	glm::vec3 padding;
};



struct GPUSceneDataMultiDraw {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
	glm::vec4 cameraPosition;
};

struct MultiDrawBuffers {
	AllocatedBuffer indirectDrawBuffer;
	uint32_t instanceCount;
};

// Material Structure
enum class MaterialPass :uint8_t {
	MainColor = 1,
	Transparent = 2,
	Other = 3,
};


// base class for a renderable dynamic object
class IRenderable { };

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
		for (auto& c : children) {
			c->refreshTransform(worldTransform);
		}
	}

	virtual ~Node() = default;
};

struct MeshNodeMultiDraw : public Node {
	uint32_t meshIndex;
	int instanceIndex{ };

};



#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::print("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
