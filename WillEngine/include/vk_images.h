#pragma once
#include <vulkan/vulkan.h>

namespace vkutil {
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout);
	void transition_image_optimized_one(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout);
	void transition_image_optimized_two(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout targetLayout);
}

