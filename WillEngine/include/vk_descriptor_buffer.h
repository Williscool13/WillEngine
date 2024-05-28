#pragma once

#include <vk_types.h>
class DescriptorBuffer {
public:
	// buffer w/ layout specified by descriptorSetLayout
	AllocatedBuffer descriptor_buffer;
	VkDescriptorSetLayout descriptor_set_layout{ VK_NULL_HANDLE };

	// total size of layout is at least sum of all bindings
	//   but it can be larger due to potential metadata or pading from driver implementation
	VkDeviceSize descriptor_buffer_size;
	VkDeviceSize descriptor_buffer_offset;


	// defined functions (define in init)
	PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSizeEXT;
	PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffsetEXT;
	PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT;
	PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT;
	PFN_vkGetDescriptorEXT vkGetDescriptorEXT;

	VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{};

	bool is_buffer_mapped = false;
	bool descriptor_set_layout_is_set = false;
	void* buffer_ptr;

	void init(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice);
	void setup_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
	void destroy(VkDevice device, VmaAllocator allocator);

private:
	inline VkDeviceSize aligned_size(VkDeviceSize value, VkDeviceSize alignment);
};

// For any descriptor type that does not require a sampler
class DescriptorBufferUniform : public DescriptorBuffer {
};


// samplers and combined images need additional flags
//   VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
// for simplicity, will also allow sampled images and storage images
class DescriptorBufferSampler : public DescriptorBuffer {
public:

	void prepare_buffer(VmaAllocator allocator);
	void setup_data(VkDevice device, std::vector<std::pair<VkDescriptorType, VkDescriptorImageInfo>>);
	void bind(VkCommandBuffer cmd, VkDevice device, VkPipelineLayout pipeline_layout);
};