#pragma once

#include <vk_types.h>

class DescriptorBuffer {
public:
	DescriptorBuffer() = default;
	DescriptorBuffer(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device
		, VmaAllocator allocator, VkDescriptorSetLayout descriptor_set_layout, int maxObjectCount = 10);

	void destroy(VkDevice device, VmaAllocator allocator);

	VkDeviceSize descriptor_buffer_size;
	VkDeviceSize descriptor_buffer_offset;

protected:
	inline VkDeviceSize aligned_size(VkDeviceSize value, VkDeviceSize alignment);
	inline VkDeviceAddress get_device_address(VkDevice device, VkBuffer buffer);

	// buffer w/ layout specified by descriptorSetLayout
	AllocatedBuffer descriptor_buffer;
	VkDescriptorSetLayout descriptor_set_layout;
	
	// total size of layout is at least sum of all bindings
	//   but it can be larger due to potential metadata or pading from driver implementationVkDeviceSize descriptor_buffer_offset;

	std::stack<int> free_indices;


	// static these things cause they are the same for all instances.
	//  staticing reduces size from 400 to 112 bytes
	static VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties;
	static PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSizeEXT;
	static PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffsetEXT;
	static PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT;
	static PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT;
	static PFN_vkGetDescriptorEXT vkGetDescriptorEXT;

	static bool device_properties_retrieved;
	static bool extension_functions_defined;
	bool is_buffer_mapped = false;
	void* buffer_ptr;
};

// For any descriptor type that does not require a sampler
class DescriptorBufferUniform : public DescriptorBuffer {
public:
	DescriptorBufferUniform() = default;
	DescriptorBufferUniform(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice
		, VmaAllocator allocator, VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

	int setup_data(VkDevice device, const AllocatedBuffer& uniform_buffer, size_t allocSize);
	VkDescriptorBufferBindingInfoEXT get_descriptor_buffer_binding_info(VkDevice device);
};


// samplers and combined images need additional flags
//   VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
// for simplicity, will also allow sampled images and storage images
class DescriptorBufferSampler : public DescriptorBuffer {
public:
	DescriptorBufferSampler() = default;
	DescriptorBufferSampler(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice
		, VmaAllocator allocator, VkDescriptorSetLayout descriptorSetLayout, int maxObjectCount = 10);

	int setup_data(VkDevice device, std::vector<std::pair<VkDescriptorType, VkDescriptorImageInfo>> data);
	VkDescriptorBufferBindingInfoEXT get_descriptor_buffer_binding_info(VkDevice device);
};
