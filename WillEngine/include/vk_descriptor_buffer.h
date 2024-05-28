#pragma once

#include <vk_types.h>

class DescriptorBuffer {
public:
	DescriptorBuffer() = default;
	DescriptorBuffer(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator, VkDescriptorSetLayout descriptor_set_layout);

	void destroy(VkDevice device, VmaAllocator allocator);

protected:
	inline VkDeviceSize aligned_size(VkDeviceSize value, VkDeviceSize alignment);
	inline VkDeviceAddress get_device_address(VkDevice device, VkBuffer buffer);

	// buffer w/ layout specified by descriptorSetLayout
	AllocatedBuffer descriptor_buffer;
	VkDescriptorSetLayout descriptor_set_layout;
	VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{};

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

	bool is_initialized = false;
	bool is_buffer_mapped = false;
	void* buffer_ptr;
};

// For any descriptor type that does not require a sampler
class DescriptorBufferUniform : public DescriptorBuffer {
public:
	DescriptorBufferUniform() = default;
	DescriptorBufferUniform(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator, VkDescriptorSetLayout descriptorSetLayout);

	void setup_data(VkDevice device, AllocatedBuffer& uniform_buffer, size_t allocSize);
	VkDescriptorBufferBindingInfoEXT get_descriptor_buffer_binding_info(VkDevice device);
};


// samplers and combined images need additional flags
//   VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
// for simplicity, will also allow sampled images and storage images
class DescriptorBufferSampler : public DescriptorBuffer {
public:
	DescriptorBufferSampler() = default;
	DescriptorBufferSampler(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator, VkDescriptorSetLayout descriptorSetLayout);

	void setup_data(VkDevice device, std::vector<std::pair<VkDescriptorType, VkDescriptorImageInfo>> data);
	VkDescriptorBufferBindingInfoEXT get_descriptor_buffer_binding_info(VkDevice device);
};
