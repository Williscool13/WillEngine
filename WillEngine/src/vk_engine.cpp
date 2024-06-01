﻿//> includes
#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_vulkan.h>


#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <stb_image/stb_image_write.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

#include <chrono>
#include <thread>

#include <vkbootstrap/VkBootstrap.h>

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif


//bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

inline VkDeviceSize aligned_size(VkDeviceSize value, VkDeviceSize alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	_window = SDL_CreateWindow(
		"Will Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags);

	TimeUtil::Get().init();
	InputManager::Get().init();

	init_vulkan();

	init_swapchain();

	init_commands();

	init_sync_structures();
	
	init_descriptors();

	init_pipelines();

	init_dearimgui();

	init_default_data();

	std::string structurePath = { "assets\\models\\structure.glb" };
	auto structureFile = loadGltf(this, structurePath);
	assert(structureFile.has_value());
	loadedScenes["structure"] = *structureFile;
	mainCamera.position = glm::vec3(30.f, -00.f, -085.f);
	mainCamera.yaw = -90.0f;

	_isInitialized = true;

}

// Output: Instance, Physical Device, Device, Surface, Graphics Queue, Graphics Queue Family, Allocator
void VulkanEngine::init_vulkan()
{
	vkb::InstanceBuilder builder;

	// make the vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Will's Vulkan Renderer")
		.request_validation_layers(USE_VALIDATION_LAYERS)
		.use_default_debug_messenger()
		.require_api_version(1, 3)
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	// vulkan instance
	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;

	// sdl vulkan surface
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	

	// vk 1.3
	VkPhysicalDeviceVulkan13Features features{};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features.dynamicRendering = true;
	features.synchronization2 = true;

	// vk 1.2
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	// Descriptor Buffer Extension
	VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
	descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
	descriptorBufferFeatures.pNext = nullptr;
	descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
	descriptorBufferFeatures.descriptorBufferCaptureReplay = VK_FALSE;
	descriptorBufferFeatures.descriptorBufferImageLayoutIgnored = VK_FALSE;

	// select gpu
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice targetDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
		.set_surface(_surface)
		.select()
		.value();
	
	vkb::DeviceBuilder deviceBuilder{ targetDevice };
	deviceBuilder.add_pNext(&descriptorBufferFeatures);
	vkb::Device vkbDevice = deviceBuilder.build().value();

	_device = vkbDevice.device;
	_physicalDevice = targetDevice.physical_device;

	vkCmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)vkGetDeviceProcAddr(_device, "vkCmdBindDescriptorBuffersEXT");
	vkCmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)vkGetDeviceProcAddr(_device, "vkCmdSetDescriptorBufferOffsetsEXT");

	// Graphics Queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	



	// VMA
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _physicalDevice;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.push_function([&]() {
		vmaDestroyAllocator(_allocator);
		});
}

void VulkanEngine::init_swapchain()
{
	create_swapchain(_windowExtent.width, _windowExtent.height);

	// Draw Image
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkExtent3D drawImageExtent = { _windowExtent.width, _windowExtent.height, 1 };
	_drawImage.imageExtent = drawImageExtent;
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);// only found on GPU
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	// Depth Image
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	// Create image views
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image
		, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image
		, VK_IMAGE_ASPECT_DEPTH_BIT);
	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

	// Deletion Queue
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
		});
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
		});
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
	vkb::SwapchainBuilder swapchainBuilder{ _physicalDevice,_device,_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchainExtent = vkbSwapchain.extent;

	// Swapchain and SwapchainImages
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

}

void VulkanEngine::destroy_swapchain() {
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	for (int i = 0; i < _swapchainImageViews.size(); i++) {
		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::init_commands()
{
	VkCommandPoolCreateInfo commandPoolInfo =
		vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		VkCommandBufferAllocateInfo cmdAllocInfo =
			vkinit::command_buffer_allocate_info(_frames[i]._commandPool);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
	}

	// Immediate Rendering
	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
	VkCommandBufferAllocateInfo immCmdAllocInfo =
		vkinit::command_buffer_allocate_info(_immCommandPool);
	VK_CHECK(vkAllocateCommandBuffers(_device, &immCmdAllocInfo, &_immCommandBuffer));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _immCommandPool, nullptr);
		});
}

void VulkanEngine::init_sync_structures()
{
	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
	}

	// Immediate Rendeirng
	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _immFence, nullptr);
		});
}

void VulkanEngine::init_descriptors()
{
#pragma region Compute Pipelien Draw Image Descriptor Buffer
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		computeImageDescriptorSetLayout = builder.build(
			_device, VK_SHADER_STAGE_COMPUTE_BIT,
			nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
		);
	}

	VkDescriptorImageInfo drawImageDescriptor{};
	drawImageDescriptor.imageView = _drawImage.imageView;
	drawImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// needs to match the order of the bindings in the layout
	std::vector<std::pair<VkDescriptorType, VkDescriptorImageInfo>> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, drawImageDescriptor }
	};

	computeImageDescriptorBuffer = DescriptorBufferSampler(_instance, _device
		, _physicalDevice, _allocator, computeImageDescriptorSetLayout);

	computeImageDescriptorBuffer.setup_data(_device, combined_descriptor);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, computeImageDescriptorSetLayout, nullptr);
		computeImageDescriptorBuffer.destroy(_device, _allocator);
		});
#pragma endregion

#pragma region Scene Data Descriptor Buffer
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		gpuSceneDataDescriptorBufferSetLayout = builder.build(_device
			, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr
			, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
		);
	}

	gpuSceneDataDescriptorBuffer = DescriptorBufferUniform(_instance, _device, _physicalDevice, _allocator, gpuSceneDataDescriptorBufferSetLayout);
	gpuSceneDataBuffer =
		create_buffer(sizeof(GPUSceneData)
			, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			, VMA_MEMORY_USAGE_CPU_TO_GPU
		);

	gpuSceneDataDescriptorBuffer.setup_data(_device, gpuSceneDataBuffer, sizeof(GPUSceneData));
	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, gpuSceneDataDescriptorBufferSetLayout, nullptr);
		gpuSceneDataDescriptorBuffer.destroy(_device, _allocator);
		destroy_buffer(gpuSceneDataBuffer);
		});

#pragma endregion
}

void VulkanEngine::update_scene()
{
	auto start = std::chrono::system_clock::now();

	mainDrawContext.OpaqueSurfaces.clear();
	mainDrawContext.TransparentSurfaces.clear();

	mainCamera.update();
	glm::mat4 view = mainCamera.getViewMatrix();
	//glm::mat4 view = glm::lookAt(glm::vec3(0, 0, camera_dist), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 proj = glm::perspective(glm::radians(70.0f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.0f, 0.1f);
	proj[1][1] *= -1;
	sceneData.view = view;
	sceneData.proj = proj;
	sceneData.viewproj = sceneData.proj * sceneData.view;

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.1f);
	sceneData.sunlightColor = glm::vec4(1.f);
	sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);

	// writing directly, if larger data, use staging buffer
	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	memcpy(sceneUniformData, &sceneData, sizeof(GPUSceneData));

	glm::mat4 structures_model = glm::scale(glm::vec3(globalModelScale));
	loadedScenes["structure"]->Draw(structures_model, mainDrawContext);


	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time = elapsed.count() / 1000.f;
}


void VulkanEngine::init_pipelines()
{
	init_compute_pipelines();
	metallicRoughnessPipelines.build_pipelines(this);

	_mainDeletionQueue.push_function([&]() {
		metallicRoughnessPipelines.destroy(_device, _allocator);
		});
}

void VulkanEngine::init_dearimgui()
{
	// DYNAMIC RENDERING (NOT RENDER PASS)
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	};
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForVulkan(_window);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _physicalDevice;
	init_info.Device = _device;
	init_info.QueueFamily = _graphicsQueueFamily;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.Subpass = 0;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	//dynamic rendering parameters for imgui to use
	init_info.UseDynamicRendering = true;
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

	ImGui_ImplVulkan_Init(&init_info);
	ImGui_ImplVulkan_CreateFontsTexture();


	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		});
}

void VulkanEngine::init_compute_pipelines()
{
	// Layout
	//  Descriptors
	VkPipelineLayoutCreateInfo backgroundEffectLayoutCreateInfo{};
	backgroundEffectLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	backgroundEffectLayoutCreateInfo.pNext = nullptr;
	backgroundEffectLayoutCreateInfo.pSetLayouts = &computeImageDescriptorSetLayout;
	backgroundEffectLayoutCreateInfo.setLayoutCount = 1;
	//  Push Constants
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	backgroundEffectLayoutCreateInfo.pPushConstantRanges = &pushConstant;
	backgroundEffectLayoutCreateInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &backgroundEffectLayoutCreateInfo, nullptr, &_backgroundEffectPipelineLayout));

	// Pipeline
	VkShaderModule gradientShader;
	if (!vkutil::load_shader_module("shaders/gradient_color.comp.spv", _device, &gradientShader)) {
		fmt::print("Error when building the compute shader (gradient_color.comp.spv)\n"); abort();
	}
	VkShaderModule skyShader;
	if (!vkutil::load_shader_module("shaders/sky.comp.spv", _device, &skyShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main"; // entry point in shader

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _backgroundEffectPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;
	computePipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

	ComputeEffect gradient;
	gradient.name = "gradient_color";
	gradient.layout = _backgroundEffectPipelineLayout;
	gradient._data = {};
	gradient._data.data1 = glm::vec4(1, 0, 0, 1);
	gradient._data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = _backgroundEffectPipelineLayout;
	sky.name = "sky";
	sky._data = {};
	//default sky parameters
	sky._data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));


	backgroundEffects = { gradient, sky };

	// Cleanup
	vkDestroyShaderModule(_device, gradientShader, nullptr);
	vkDestroyShaderModule(_device, skyShader, nullptr);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, _backgroundEffectPipelineLayout, nullptr);
		vkDestroyPipeline(_device, sky.pipeline, nullptr);
		vkDestroyPipeline(_device, gradient.pipeline, nullptr);
		});
}

void VulkanEngine::init_default_data()
{
#pragma region Basic Textures
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);
#pragma endregion

#pragma region Default Samplers
	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

	_mainDeletionQueue.push_function([&]() {
		destroy_image(_whiteImage);
		destroy_image(_greyImage);
		destroy_image(_blackImage);
		destroy_image(_errorCheckerboardImage);
		vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
		vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
		});
#pragma endregion

#pragma region Default Material
	GLTFMetallic_Roughness::MaterialResources materialResources;
	//default the material textures
	materialResources.colorImage = _whiteImage;
	materialResources.colorSampler = _defaultSamplerLinear;
	materialResources.metalRoughImage = _whiteImage;
	materialResources.metalRoughSampler = _defaultSamplerLinear; 

	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = 
		create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants)
			, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			, VMA_MEMORY_USAGE_CPU_TO_GPU);  

	// Not modified in current build
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = 
		(GLTFMetallic_Roughness::MaterialConstants*)materialConstants.info.pMappedData;
	sceneUniformData->colorFactors = glm::vec4{ 1,1,1,1 };
	sceneUniformData->metal_rough_factors = glm::vec4{ 1,0.5,0,0 };

	_mainDeletionQueue.push_function([=, this]() {
		destroy_buffer(materialConstants);
		});

	materialResources.dataBuffer = materialConstants;
	materialResources.dataBufferSize = sizeof(GLTFMetallic_Roughness::MaterialConstants);
	//materialResources.dataBufferOffset = 0;

	defaultOpaqueMaterial = metallicRoughnessPipelines.write_material(
		_device, MaterialPass::MainColor, materialResources);

#pragma endregion
}

// Creation Order (cleaned up in opposite order)
// Window -> Vulkan Instance -> SDL Surface -> Physical Device -> Device -> Swapchain -> Command Pool -> Command Buffer
//    cant delete physical device, because its just a handle to the GPU - Implicitly destroyed when the instance is destroyed
//    cant individually destroy command buffers, deleting command pool deletes all allocated command buffers by that pool
void VulkanEngine::cleanup()
{
	if (_isInitialized) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		vkDeviceWaitIdle(_device);
		loadedScenes.clear();

		_mainDeletionQueue.flush();

		for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
		}

		destroy_swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);


		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}

	// clear engine pointer
	loadedEngine = nullptr;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
	ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];
	// Bind Pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, selected.pipeline);
	// Push Constants
	vkCmdPushConstants(cmd, _backgroundEffectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &selected._data);


	VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[1]{};
	descriptor_buffer_binding_info[0] = computeImageDescriptorBuffer.get_descriptor_buffer_binding_info(_device);
	vkCmdBindDescriptorBuffersEXT(cmd, 1, descriptor_buffer_binding_info);
	uint32_t buffer_index_image = 0;
	VkDeviceSize buffer_offset = 0;

	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _backgroundEffectPipelineLayout
		, 0, 1, &buffer_index_image, &buffer_offset); 


	// Execute at 8x8 thread groups
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 8.0), std::ceil(_drawExtent.height / 8.0), 1);


}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
	//reset counters
	stats.drawcall_count = 0;
	stats.triangle_count = 0;
	//begin clock
	auto start = std::chrono::system_clock::now();


	VkClearValue depthClearValue = { 0.0f, 0 };
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::attachment_info(_depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	auto draw = [&](const RenderObject& draw) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
		// Dynamic States
		{
			//  Viewport
			VkViewport viewport = {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = _drawExtent.width;
			viewport.height = _drawExtent.height;
			viewport.minDepth = 0.f;
			viewport.maxDepth = 1.f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			//  Scissor
			VkRect2D scissor = {};
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = _drawExtent.width;
			scissor.extent.height = _drawExtent.height;
			vkCmdSetScissor(cmd, 0, 1, &scissor);
		}
		 
		VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[3]{};
		descriptor_buffer_binding_info[0] = gpuSceneDataDescriptorBuffer.get_descriptor_buffer_binding_info(_device);
		descriptor_buffer_binding_info[1] = draw.material->pipeline->materialTextureDescriptorBuffer->get_descriptor_buffer_binding_info(_device);
		descriptor_buffer_binding_info[2] = draw.material->pipeline->materialUniformDescriptorBuffer->get_descriptor_buffer_binding_info(_device);
		vkCmdBindDescriptorBuffersEXT(cmd, 3, descriptor_buffer_binding_info);
		uint32_t buffer_index_ubo = 0;
		uint32_t buffer_index_image = 1;
		uint32_t buffer_index_material = 2;
		VkDeviceSize global_buffer_offset = 0;
		VkDeviceSize texture_buffer_offset = draw.material->textureDescriptorBufferIndex * draw.material->pipeline->materialTextureDescriptorBuffer->descriptor_buffer_size;
		VkDeviceSize uniform_buffer_offset = draw.material->uniformDescriptorBufferIndex * draw.material->pipeline->materialUniformDescriptorBuffer->descriptor_buffer_size;

		vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout
			, 0, 1, &buffer_index_ubo, &global_buffer_offset);
		vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout
			, 1, 1, &buffer_index_image, &texture_buffer_offset);
		vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout
			, 2, 1, &buffer_index_material, &uniform_buffer_offset);

		vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		GPUDrawPushConstants pushConstants;
		pushConstants.vertexBuffer = draw.vertexBufferAddress;
		pushConstants.modelMatrix = draw.transform;
		pushConstants.invTransposeModelMatrix = glm::transpose(glm::inverse(glm::mat3(draw.transform)));
		vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);


		//add counters for triangles and draws
		stats.drawcall_count++;
		stats.triangle_count += draw.indexCount / 3;
	};

	for (auto& r : mainDrawContext.OpaqueSurfaces) {
		draw(r);
	}
	for (auto& r : mainDrawContext.TransparentSurfaces) {
		draw(r);
	}

	vkCmdEndRendering(cmd);

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.mesh_draw_time = elapsed.count() / 1000.f;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &renderInfo);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRendering(cmd);
}


void VulkanEngine::draw()
{
	update_scene();

	// GPU -> VPU sync (fence)
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
	get_current_frame()._deletionQueue.flush();
	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

	// GPU -> GPU sync (semaphore)
	uint32_t swapchainImageIndex;
	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) { resize_requested = true; fmt::print("Swapchain out of date, resize requested\n"); return; }

	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // only submit once


	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * _renderScale;
	_drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * _renderScale;


	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// draw background onto _drawImage
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	draw_background(cmd);
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	draw_geometry(cmd);

	// copy _drawImage onto _swapchainImage
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

	// Draw imgui onto _swapchainImage (directly)
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

	// transition to present 
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	// Submission
	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR, get_current_frame()._renderSemaphore);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	// submit cmd to queue, signals fence when done
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));


	// Present
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		fmt::print("present failed - out of date, resize requested\n");
	}

	//increase the number of frames drawn
	_frameNumber++;


}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit) {
		// Handle events on queue
		 //begin clock
		auto start = std::chrono::system_clock::now();

		InputManager::Get().frame_reset();

		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) { bQuit = true; continue; }
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) { SDL_SetRelativeMouseMode(SDL_FALSE); }
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_t) { SDL_SetRelativeMouseMode(SDL_TRUE); }
			if (e.type == SDL_WINDOWEVENT) {
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					stop_rendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					stop_rendering = false;
				}
			}

			InputManager::Get().update(&e);

			// imgui input handling
			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		// Delta Time
		TimeUtil::Get().update();


		Uint32 windowFlags = SDL_GetWindowFlags(_window);
		bool isWindowInFocus = (windowFlags & SDL_WINDOW_INPUT_FOCUS) != 0;

		if (InputManager::Get().isKeyPressed(InputManager::Key::ESCAPE)) {
			if (mouseLocked) { SDL_SetRelativeMouseMode(SDL_FALSE); mouseLocked = false; }
			else { bQuit = true; continue; }
		}

		if (!mouseLocked && isWindowInFocus && InputManager::Get().isMousePressed(InputManager::MouseKey::RIGHT)) {
			SDL_SetRelativeMouseMode(SDL_TRUE); 
			mouseLocked = true;
		}

		
		// Camera Input Handling
		mainCamera.processSDLEvent(isWindowInFocus && mouseLocked);

		if (resize_requested) {
			resize_swapchain();
		}

		// do not draw if we are minimized
		if (stop_rendering) {
			// throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();

		if (ImGui::Begin("background")) {

			ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

			ImGui::InputFloat4("data1", (float*)&selected._data.data1);
			ImGui::InputFloat4("data2", (float*)&selected._data.data2);


			ImGui::SliderFloat("Render Scale", &_renderScale, 0.1f, _maxRenderScale);

			ImGui::SliderFloat("Model Scale", &globalModelScale, 0.1f, 20.0f);

			if (ImGui::BeginChild("Stats")) {
				ImGui::Text("frametime %f ms", stats.frametime);
				ImGui::Text("draw time %f ms", stats.mesh_draw_time);
				ImGui::Text("update time %f ms", stats.scene_update_time);
				ImGui::Text("triangles %i", stats.triangle_count);
				ImGui::Text("draws %i", stats.drawcall_count);
			}

			ImGui::EndChild();

		}
		ImGui::End();
		ImGui::Render();

		draw();


		//everything else

			//get clock again, compare with start clock
		auto end = std::chrono::system_clock::now();

		//convert to microseconds (integer), and then come back to miliseconds
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count() / 1000.f;
	}
}

void VulkanEngine::resize_swapchain() {
	vkDeviceWaitIdle(_device);

	destroy_swapchain();

	int w, h;
	// get new window size
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	_maxRenderScale = std::min(
		(float)_drawImage.imageExtent.width / (float)_windowExtent.width
		, (float)_drawImage.imageExtent.height / (float)_windowExtent.height
	);
	_maxRenderScale = std::max(_maxRenderScale, 1.0f);

	_renderScale = std::min(_maxRenderScale, _renderScale);

	create_swapchain(_windowExtent.width, _windowExtent.height);

	resize_requested = false;

}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{

	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;

	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;


	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	// Create Buffers on GPU
	newSurface.vertexBuffer = create_buffer(vertexBufferSize
		, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		, VMA_MEMORY_USAGE_GPU_ONLY);
	newSurface.indexBuffer = create_buffer(indexBufferSize
		, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	//find the adress of the vertex buffer
	VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);


	// Allocate Data. Intermediate Buffer w/ CPU Access
	AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize
		, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		, VMA_MEMORY_USAGE_CPU_ONLY);
	void* data = staging.allocation->GetMappedData();
	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer (offset by vertex buffer size)
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
		});

	destroy_buffer(staging);

	return newSurface;


}


void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBuffer cmd = _immCommandBuffer;
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	function(cmd);
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdSubmitInfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdSubmitInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 1000000000));
}


#pragma region TEXTURES
AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadbuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadbuffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	immediate_submit([&](VkCommandBuffer cmd) {
		vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});

	destroy_buffer(uploadbuffer);

	return new_image;
}

void VulkanEngine::destroy_image(const AllocatedImage& img)
{
	vkDestroyImageView(_device, img.imageView, nullptr);
	vmaDestroyImage(_allocator, img.image, img.allocation);
}
#pragma endregion


#pragma region PIPELINES
void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule meshFragShader;
	if (!vkutil::load_shader_module("shaders/mesh.frag.spv", engine->_device, &meshFragShader)) {
		fmt::print("Error when building the triangle fragment shader module\n");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module("shaders/mesh.vert.spv", engine->_device, &meshVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Defining Descriptor Layouts
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		materialUniformLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	}
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_SAMPLER);
		layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_SAMPLER);
		layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);


		materialTextureLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	}


	// Defining Descriptor Buffers
	{
		materialTextureDescriptorBuffer = DescriptorBufferSampler(engine->_instance, engine->_device
			, engine->_physicalDevice, engine->_allocator, materialTextureLayout, 50);
		materialUniformDescriptorBuffer = DescriptorBufferUniform(engine->_instance, engine->_device
			, engine->_physicalDevice, engine->_allocator, materialUniformLayout, 50);
	}

	pipeline_layout_initialized = true;

	VkDescriptorSetLayout layouts[] = { 
		engine->gpuSceneDataDescriptorBufferSetLayout
		, materialTextureLayout
		, materialUniformLayout
	};	 

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 3;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;
	
	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;
	pipelineLayout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.setup_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.setup_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.disable_multisampling();
	pipelineBuilder.setup_blending(PipelineBuilder::BlendMode::NO_BLEND);
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	//render format
	pipelineBuilder.setup_renderer(engine->_drawImage.imageFormat, engine->_depthImage.imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

	// Opaque Pipeline Descriptor Buffer Pointer
	{
		opaquePipeline.materialTextureDescriptorBuffer = &materialTextureDescriptorBuffer;
		opaquePipeline.materialUniformDescriptorBuffer = &materialUniformDescriptorBuffer;
	}

	// create the transparent variant
	pipelineBuilder.setup_blending(PipelineBuilder::BlendMode::ADDITIVE_BLEND);

	pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	
	// Transparent Pipeline Descriptor Buffer Creation
	{
		transparentPipeline.materialTextureDescriptorBuffer = &materialTextureDescriptorBuffer;
		transparentPipeline.materialUniformDescriptorBuffer = &materialUniformDescriptorBuffer;
	}

	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(
	VkDevice device
	, MaterialPass pass
	, const MaterialResources& resources
) {
	if (!pipeline_layout_initialized) {
		fmt::print("Error: Pipeline Layout not initialized\n");
		return {};
	}

	MaterialInstance matData;
	matData.passType = pass;
	if (pass == MaterialPass::Transparent) {
		matData.pipeline = &transparentPipeline;
	}
	else {
		matData.pipeline = &opaquePipeline;
	}

	VkDescriptorImageInfo colorCombinedDescriptor{};
	colorCombinedDescriptor.sampler = resources.colorSampler;
	colorCombinedDescriptor.imageView = resources.colorImage.imageView;
	colorCombinedDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo metalRoughCombinedDescriptor{};
	metalRoughCombinedDescriptor.sampler = resources.metalRoughSampler;
	metalRoughCombinedDescriptor.imageView = resources.metalRoughImage.imageView;
	metalRoughCombinedDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


	// needs to match the order of the bindings in the layout
	std::vector<std::pair<VkDescriptorType, VkDescriptorImageInfo>> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, colorCombinedDescriptor },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, metalRoughCombinedDescriptor },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, colorCombinedDescriptor },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, metalRoughCombinedDescriptor}
	};
	

	matData.colorDescriptorImageInfo 		 = colorCombinedDescriptor;
	matData.metalRoughDescriptorImageInfo	 = metalRoughCombinedDescriptor;
	matData.materialUniformBuffer			 = resources.dataBuffer;


	matData.textureDescriptorBufferIndex = matData.pipeline->materialTextureDescriptorBuffer->setup_data(
		device, combined_descriptor
	);
	matData.uniformDescriptorBufferIndex = matData.pipeline->materialUniformDescriptorBuffer->setup_data(
		device, resources.dataBuffer, resources.dataBufferSize
	);



	return matData;
}


void GLTFMetallic_Roughness::destroy(VkDevice device, VmaAllocator allocator)
{
	vkDestroyDescriptorSetLayout(device, materialTextureLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, materialUniformLayout, nullptr);
	materialTextureDescriptorBuffer.destroy(device, allocator);
	materialUniformDescriptorBuffer.destroy(device, allocator);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
	vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
}



#pragma endregion

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	glm::mat4 nodeMatrix = topMatrix * worldTransform;

	for (auto& s : mesh->surfaces) {
		RenderObject def;
		def.indexCount = s.count;
		def.firstIndex = s.startIndex;
		def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
		def.material = &s.material->data;

		def.transform = nodeMatrix;
		def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

		if (s.material->data.passType == MaterialPass::Transparent) {
			ctx.TransparentSurfaces.push_back(def);
		}
		else {
			ctx.OpaqueSurfaces.push_back(def);
		}
	}

	// recurse down
	Node::Draw(topMatrix, ctx);
}
