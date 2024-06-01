﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <stack>
#include <variant>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#include <vma/vk_mem_alloc.h>

#include <fmt/core.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <time_util.h>
#include <input_manager.h>

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
}; 


// Material Structure
enum class MaterialPass :uint8_t {
	MainColor,
	Transparent,
	Other
};
class DescriptorBufferUniform;
class DescriptorBufferSampler;

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	DescriptorBufferSampler* materialTextureDescriptorBuffer;
	DescriptorBufferUniform* materialUniformDescriptorBuffer;
};

struct MaterialInstance {
	MaterialPipeline* pipeline;
	// descriptor properties
	VkDescriptorImageInfo colorDescriptorImageInfo;
	VkDescriptorImageInfo metalRoughDescriptorImageInfo;
	AllocatedBuffer materialUniformBuffer;

	int textureDescriptorBufferIndex;
	int uniformDescriptorBufferIndex;
	MaterialPass passType;
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
