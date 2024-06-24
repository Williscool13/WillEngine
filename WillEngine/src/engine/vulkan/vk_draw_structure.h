#pragma once
#include "will_engine.h"
#include "vk_types.h"

#include "vk_engine.h"
#include "vk_descriptor_buffer.h"
#include "vk_loader.h"

class VulkanEngine;
struct LoadedGLTFMultiDraw;

struct MultiDrawBuffers {
	AllocatedBuffer indirectDrawBuffer;
	uint32_t instanceCount;
};

struct GLTFMetallic_RoughnessMultiDraw {
	bool hasTransparents() const;
	bool hasOpaques() const;

	VulkanEngine* creator;

	std::shared_ptr<LoadedGLTFMultiDraw> scene_ptr;
	std::shared_ptr<ShaderObject> shaderObject;

	VkPipelineLayout renderPipelineLayout;

	static VkDescriptorSetLayout bufferAddressesDescriptorSetLayout;
	static VkDescriptorSetLayout textureDescriptorSetLayout;
	static VkDescriptorSetLayout computeCullingDescriptorSetLayout;
	static VkPipelineLayout _computeCullingPipelineLayout;
	static VkPipeline _computeCullingPipeline;
	static bool layoutsCreated;
	static int useCount;

	AllocatedBuffer indexBuffer;

	// BINDING 0: BUFFER ADDRESSES
	DescriptorBufferUniform buffer_addresses;
	AllocatedBuffer buffer_addresses_underlying;
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer materialBuffer;
	AllocatedBuffer instanceBuffer;
	// access correct instance data w/ gl_instanceID. 

	// BINDING 1: TEXTURE DATA
	//	when initializing, set descriptor count to be equal to the number of textures
	DescriptorBufferSampler texture_data;

	// BINDING 2: SCENE DATA
	//DescriptorBufferUniform scene_data;
	//AllocatedBuffer sceneDataBuffer;


	// BINDING 1: INDIRECT DRAW BUFFER
	DescriptorBufferUniform compute_culling_data_buffer_address;
	AllocatedBuffer indirect_draw_buffer_underlying;
	MultiDrawBuffers opaqueDrawBuffers{ VK_NULL_HANDLE, 0 };
	MultiDrawBuffers transparentDrawBuffers{ VK_NULL_HANDLE, 0 };
	AllocatedBuffer boundingSphereBuffer;


	GLTFMetallic_RoughnessMultiDraw() = delete;
	GLTFMetallic_RoughnessMultiDraw(VulkanEngine* engine, std::string& pathToScene, const char* vertShaderPath, const char* fragShaderPath, bool use_msaa, VkSampleCountFlagBits sample_count);
	~GLTFMetallic_RoughnessMultiDraw();

	// buffer building
	size_t index_buffer_size = 0;
	size_t number_of_instances = 0;
	std::vector<InstanceData> instanceData;
	std::vector<MeshData> meshData;
	// store offsets of vertices
	std::vector<uint32_t> vertexOffsets;

	void build_buffers(VulkanEngine* engine);
	void recursive_node_process(LoadedGLTFMultiDraw& scene, Node& node, glm::mat4& topMatrix);
	void recursive_node_process_instance_data(LoadedGLTFMultiDraw& scene, Node& node, glm::mat4& topMatrix, int& current_model_index);
	void update_model_matrix(glm::mat4& topMatrix);

	bool buffersBuilt{ false };

	const uint32_t buffer_addresses_descriptor_index = 0;
	const uint32_t texture_data_descriptor_index = 1;
	const uint32_t scene_data_descriptor_index = 2;
	const uint32_t compute_culling_data_descriptor_index = 1;
	const VkDeviceSize offsets = 0;
	void cull(VkCommandBuffer cmd);
	void draw(VkCommandBuffer cmd, VkExtent2D drawExtents);
};
