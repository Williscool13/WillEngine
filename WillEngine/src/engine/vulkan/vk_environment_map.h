#pragma once
#include "will_engine.h"
#include "vk_types.h"
#include "vk_engine.h"

#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "vk_descriptor_buffer.h"

#include <stb_image/stb_image.h>
#include <stb_image/stb_image_write.h>


struct EquiToCubePushConstantData {
	bool flipY;
	float pad;
	float pad2;
	float pad3;
};

struct CubeToDiffusePushConstantData {
	float sampleDelta;
	float pad;
	float pad2;
	float pad3;
};

struct CubeToPrefilteredConstantData {
	float roughness;
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint32_t sampleCount;
};

struct EnvironmentMapData {
	std::string sourcePath;
	AllocatedImage cubemapImage;
	AllocatedImage specDiffCubemap;
};

class EnvironmentMap {
public:
	static const int specularPrefilteredMipLevels{ 10 };
	static const VkExtent3D specularPrefilteredBaseExtents;
	static const VkExtent3D lutImageExtent;
	static const int diffuseIrradianceMipLevel{ 5 };
	static const char* defaultEquiPath;


	EnvironmentMap(VulkanEngine* creator);
	~EnvironmentMap();

	// init sampler
	void load_cubemap(const char* path, int environmentMapIndex = 0);

	bool flip_y{ false };

	float diffuse_sample_delta{ 0.025f };
	int specular_sample_count{ 2048};


	static bool layoutsCreated;
	static int useCount;
	static VkDescriptorSetLayout _equiImageDescriptorSetLayout;
	static VkDescriptorSetLayout _cubemapStorageDescriptorSetLayout;
	static VkDescriptorSetLayout _cubemapDescriptorSetLayout;
	static VkDescriptorSetLayout _lutDescriptorSetLayout;

	static VkDescriptorSetLayout _environmentMapDescriptorSetLayout; // contains 2 samplers -> diffuse/spec and lut

	DescriptorBufferSampler& get_equi_image_descriptor_buffer() { return _equiImageDescriptorBuffer; }
	DescriptorBufferSampler& get_cubemap_descriptor_buffer() { return _cubemapDescriptorBuffer; }

	DescriptorBufferSampler& get_diff_spec_map_descriptor_buffer() { return _diffSpecMapDescriptorBuffer; }

private:
	VulkanEngine* _creator;
	VkDevice _device;
	VmaAllocator _allocator;

	
	DescriptorBufferSampler _equiImageDescriptorBuffer;
	// the 2 below are identical, but one is for storage and one is for sampled
	DescriptorBufferSampler _cubemapStorageDescriptorBuffer;
	DescriptorBufferSampler _cubemapDescriptorBuffer;

	static DescriptorBufferSampler _lutDescriptorBuffer;
	DescriptorBufferSampler _diffSpecMapDescriptorBuffer;

	// Pipelines
	static VkPipelineLayout _equiToCubemapPipelineLayout;
	static VkPipeline _equiToCubemapPipeline;
	
	static VkPipelineLayout _cubemapToDiffusePipelineLayout;
	static VkPipeline _cubemapToDiffusePipeline;
	static VkPipelineLayout _cubemapToSpecularPipelineLayout;
	static VkPipeline _cubemapToSpecularPipeline;

	// Hardcoded LUT generation
	static VkPipelineLayout _lutPipelineLayout;
	static VkPipeline _lutPipeline;



	const int MAX_ENVIRONMENT_MAPS{ 10 };
	EnvironmentMapData _environmentMaps[10]{
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
		{"", VK_NULL_HANDLE, VK_NULL_HANDLE},
	};

	static AllocatedImage _lutImage; // same for all environment maps

	VkSampler _sampler;

	//std::string _equiPath;
	uint32_t _cubemapResolution{ 1024 };


	void equi_to_cubemap_immediate(AllocatedImage& _cubemapImage, int _cubemapStorageDescriptorIndex);
	void cubemap_to_difffuse_specular_immediate(AllocatedCubemap& cubemapMips, int _cubemapSampleDescriptorIndex);
	void generate_lut_immediate();
};
