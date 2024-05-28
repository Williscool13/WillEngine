#include "vk_descriptor_buffer.h"

void DescriptorBuffer::init(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice) {
	// Get Descriptor Buffer Properties
	VkPhysicalDeviceProperties2KHR device_properties{};
	descriptor_buffer_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
	device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	device_properties.pNext = &descriptor_buffer_properties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &device_properties);

	vkGetDescriptorSetLayoutSizeEXT = (PFN_vkGetDescriptorSetLayoutSizeEXT)vkGetInstanceProcAddr(instance, "vkGetDescriptorSetLayoutSizeEXT");
	vkGetDescriptorSetLayoutBindingOffsetEXT = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)vkGetInstanceProcAddr(instance, "vkGetDescriptorSetLayoutBindingOffsetEXT");
	vkCmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)vkGetDeviceProcAddr(device, "vkCmdBindDescriptorBuffersEXT");
	vkCmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)vkGetDeviceProcAddr(device, "vkCmdSetDescriptorBufferOffsetsEXT");
	vkGetDescriptorEXT = (PFN_vkGetDescriptorEXT)vkGetDeviceProcAddr(device, "vkGetDescriptorEXT");
}

void DescriptorBuffer::setup_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
	this->descriptor_set_layout = descriptorSetLayout;
	// Buffer Size
	vkGetDescriptorSetLayoutSizeEXT(device, descriptorSetLayout, &descriptor_buffer_size);
	descriptor_buffer_size = aligned_size(descriptor_buffer_size, descriptor_buffer_properties.descriptorBufferOffsetAlignment);
	// Buffer Offset
	vkGetDescriptorSetLayoutBindingOffsetEXT(device, descriptorSetLayout, 0u, &descriptor_buffer_offset);

	descriptor_set_layout_is_set = true;
}

void DescriptorBuffer::destroy(VkDevice device, VmaAllocator allocator) {
	if (descriptor_set_layout_is_set) { vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr); }
	if (is_buffer_mapped) { vmaDestroyBuffer(allocator, descriptor_buffer.buffer, descriptor_buffer.allocation); }
}

VkDeviceSize DescriptorBuffer::aligned_size(VkDeviceSize value, VkDeviceSize alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

void DescriptorBufferSampler::prepare_buffer(VmaAllocator allocator) {
	// Allocate Buffer
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = descriptor_buffer_size;
	bufferInfo.usage =
		VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo
		, &descriptor_buffer.buffer
		, &descriptor_buffer.allocation
		, &descriptor_buffer.info));
	  
	buffer_ptr = descriptor_buffer.info.pMappedData;
	is_buffer_mapped = true;
}

//class Vma_Allocation_T;

void DescriptorBufferSampler::setup_data(VkDevice device, std::vector<std::pair<VkDescriptorType, VkDescriptorImageInfo>> data) {
	if (!is_buffer_mapped) { fmt::print("DescriptorBufferImage::setup_data() called on unmapped buffer\n"); return; }

	uint64_t accum_offset{};

	if (descriptor_buffer_properties.combinedImageSamplerDescriptorSingleArray == VK_FALSE) {
		// This is nonfunctional 
		// image_descriptor_info
		VkDescriptorGetInfoEXT image_descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
		image_descriptor_info.type = data[0].first;
		switch (data[0].first) {
		case VK_DESCRIPTOR_TYPE_SAMPLER: image_descriptor_info.data.pSampler = &data[0].second.sampler; break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: image_descriptor_info.data.pCombinedImageSampler = &data[0].second; break;
		}

		// descriptor_size
		size_t descriptor_size{};
		switch (data[0].first) {
		case VK_DESCRIPTOR_TYPE_SAMPLER: descriptor_size = descriptor_buffer_properties.samplerDescriptorSize; break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: descriptor_size = descriptor_buffer_properties.combinedImageSamplerDescriptorSize; break;
		default:
			fmt::print("DescriptorBufferImage::setup_data() called with a non-image descriptor type\n");
			return;
		}


		// pointer to start point
		char* buffer_ptr_offset = (char*)buffer_ptr + accum_offset;

		vkGetDescriptorEXT(
			device,
			&image_descriptor_info,
			descriptor_size,
			buffer_ptr_offset
		);

		accum_offset += descriptor_size;
		fmt::print("Added descriptor sampler of size {}\n", descriptor_size);
	}

	for (int i = 0; i < data.size(); i++) {
		// image_descriptor_info
		VkDescriptorGetInfoEXT image_descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
		image_descriptor_info.type = data[i].first;
		switch (data[i].first) {
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			image_descriptor_info.data.pSampler = &data[i].second.sampler;
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			image_descriptor_info.data.pCombinedImageSampler = &data[i].second;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			image_descriptor_info.data.pSampledImage = &data[i].second;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			image_descriptor_info.data.pStorageImage = &data[i].second;
			break;
		default:
			fmt::print("DescriptorBufferImage::setup_data() called with a non-image/sampler descriptor type\n");
			return;
		} 
		  
		// descriptor_size
		size_t descriptor_size{};
		switch (data[i].first) {
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			descriptor_size = descriptor_buffer_properties.samplerDescriptorSize;
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			descriptor_size = descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			descriptor_size = descriptor_buffer_properties.sampledImageDescriptorSize;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			descriptor_size = descriptor_buffer_properties.storageImageDescriptorSize;
			break;
		default:
			fmt::print("DescriptorBufferImage::setup_data() called with a non-image/sampler descriptor type\n");
			return;
		}


		// pointer to start point
		char* buffer_ptr_offset = (char*)buffer_ptr + accum_offset;

		vkGetDescriptorEXT(
			device,
			&image_descriptor_info,
			descriptor_size,
			buffer_ptr_offset
		);

		accum_offset += descriptor_size;
		fmt::print("Added descriptor sampler of size {}\n", descriptor_size);
	}
	fmt::print("yay! your gpu uses combinedImageSamplerDescriptorSingleArray\n");
}

void DescriptorBufferSampler::bind(VkCommandBuffer cmd, VkDevice device, VkPipelineLayout pipeline_layout) {
	VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
	buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	buffer_device_address_info.buffer = descriptor_buffer.buffer;
	VkDeviceAddress a = vkGetBufferDeviceAddress(device, &buffer_device_address_info);

	VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{};
	descriptor_buffer_binding_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
	descriptor_buffer_binding_info.address = a;
	descriptor_buffer_binding_info.usage
		= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

	vkCmdBindDescriptorBuffersEXT(cmd, 1, &descriptor_buffer_binding_info);

	uint32_t     buffer_index_image = 0;
	VkDeviceSize buffer_offset = 0;
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout
		, 0, 1, &buffer_index_image, &buffer_offset);
}
