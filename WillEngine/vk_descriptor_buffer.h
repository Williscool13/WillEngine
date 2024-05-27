#pragma once

#include <vk_types.h>

class DescriptorBuffer {

public:
	struct DescriptorData
	{
		VkDescriptorSetLayout              layout{ VK_NULL_HANDLE };
		std::unique_ptr<vkb::core::Buffer> buffer;
		VkDeviceSize                       size;
		VkDeviceSize                       offset;
	};

	DescriptorData uniform_binding_descriptor;
	DescriptorData image_binding_descriptor;


private:

};
