#include "vk_engine.h"

// defined here because needs implementation in translation unit
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <stb_image/stb_image_write.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>



#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif

#define USE_MSAA true
#define MSAA_SAMPLES VK_SAMPLE_COUNT_4_BIT


//bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }


void VulkanEngine::init()
{
	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetRelativeMouseMode(SDL_TRUE);
	loadedEngine = this;
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
	//std::string structurePath = { "assets\\models\\vokselia\\vokselia.gltf" };
	//std::string structurePath = { "assets\\models\\virtual_city\\VirtualCity.glb" };
	//std::string structurePath = { "assets\\models\\primitives\\primitives.gltf" };   
	//std::string structurePath = { "assets\\models\\AlphaBlendModeTest\\glTF-Binary\\AlphaBlendModeTest.glb" };
	auto structureFile = loadGltf(this, structurePath);
	auto test = loadGltfMultiDraw(this, structurePath);
	testMultiDraw.build_buffers(this, *test.value().get());
	assert(structureFile.has_value());
	loadedScenes["structure"] = *structureFile;
	loadedMultiDrawScenes["structure"] = *test;

	//mainCamera.position = glm::vec3(30.f, -00.f, -085.f);
	mainCamera.yaw = -90.0f;


	VkDescriptorImageInfo fullscreenCombined{};
	fullscreenCombined.sampler = _defaultSamplerNearest;
	fullscreenCombined.imageView = _errorCheckerboardImage.imageView;
	fullscreenCombined.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// needs to match the order of the bindings in the layout
	std::vector<DescriptorImageData> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &fullscreenCombined, 1 }
	};
	_fullscreenDescriptorBuffer.setup_data(_device, combined_descriptor);

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

	VkPhysicalDeviceFeatures other_features{};
	other_features.multiDrawIndirect = true;
	// Descriptor Buffer Extension
	VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
	descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
	descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
	// Shader Object
	VkPhysicalDeviceShaderObjectFeaturesEXT enabledShaderObjectFeaturesEXT{};
	enabledShaderObjectFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
	enabledShaderObjectFeaturesEXT.shaderObject = VK_TRUE;

	// select gpu
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice targetDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_required_features(other_features)
		.add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
		.add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
		.set_surface(_surface)
		.select()
		.value();

	vkb::DeviceBuilder deviceBuilder{ targetDevice };
	deviceBuilder.add_pNext(&descriptorBufferFeatures);
	deviceBuilder.add_pNext(&enabledShaderObjectFeaturesEXT);
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
	create_draw_images(_windowExtent.width, _windowExtent.height);

	_mainDeletionQueue.push_function([&]() {
		destroy_draw_iamges();
		});
}

void VulkanEngine::init_draw_images() {
	// Draw Image
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkExtent3D drawImageExtent = { _windowExtent.width, _windowExtent.height, 1 };
	_drawImage.imageExtent = drawImageExtent;
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);// only found on GPU
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	if (USE_MSAA) {
		drawImageUsages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);
		rimg_info.samples = VK_SAMPLE_COUNT_4_BIT;
		vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImageBeforeMSAA.image, &_drawImageBeforeMSAA.allocation, nullptr);
	}

	// Depth Image
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);
	if (USE_MSAA) { dimg_info.samples = VK_SAMPLE_COUNT_4_BIT; }
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	// Create image views
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image
		, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

	if (USE_MSAA) {
		VkImageViewCreateInfo rview_info_before_msaa = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImageBeforeMSAA.image
			, VK_IMAGE_ASPECT_COLOR_BIT);
		VK_CHECK(vkCreateImageView(_device, &rview_info_before_msaa, nullptr, &_drawImageBeforeMSAA.imageView));
	}

	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image
		, VK_IMAGE_ASPECT_DEPTH_BIT);
	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

	// Deletion Queue
	_mainDeletionQueue.push_function([&]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
		});
	if (USE_MSAA) {
		_mainDeletionQueue.push_function([&]() {
			vkDestroyImageView(_device, _drawImageBeforeMSAA.imageView, nullptr);
			vmaDestroyImage(_allocator, _drawImageBeforeMSAA.image, _drawImageBeforeMSAA.allocation);
			});
	}
	_mainDeletionQueue.push_function([&]() {
		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
		});
}

void VulkanEngine::create_draw_images(uint32_t width, uint32_t height) {
	// Draw Image
	{
		_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		VkExtent3D drawImageExtent = { width, height, 1 };
		_drawImage.imageExtent = drawImageExtent;
		VkImageUsageFlags drawImageUsages{};
		drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
		VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);
		VmaAllocationCreateInfo rimg_allocinfo = {};
		rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);// only found on GPU
		vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

		VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image
			, VK_IMAGE_ASPECT_COLOR_BIT);
		VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));
	}

	// MSAA pre-resolve image

	if (USE_MSAA) {
		_drawImageBeforeMSAA.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		VkExtent3D msaaImageExtent = { width, height, 1 };
		VkImageUsageFlags msaaImageUsages{};
		msaaImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		msaaImageUsages |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		VkImageCreateInfo msaaimg_info = vkinit::image_create_info(_drawImageBeforeMSAA.imageFormat, msaaImageUsages, msaaImageExtent);
		//msaaimg_info = vkinit::image_create_info(_drawImageBeforeMSAA.imageFormat, msaaImageUsages, msaaImageExtent);
		msaaimg_info.samples = MSAA_SAMPLES;
		VmaAllocationCreateInfo msaaimg_allocinfo = {};
		msaaimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		msaaimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);// only found on GPU
		vmaCreateImage(_allocator, &msaaimg_info, &msaaimg_allocinfo, &_drawImageBeforeMSAA.image, &_drawImageBeforeMSAA.allocation, nullptr);

		VkImageViewCreateInfo rview_info_before_msaa = vkinit::imageview_create_info(
			_drawImageBeforeMSAA.imageFormat, _drawImageBeforeMSAA.image, VK_IMAGE_ASPECT_COLOR_BIT);
		VK_CHECK(vkCreateImageView(_device, &rview_info_before_msaa, nullptr, &_drawImageBeforeMSAA.imageView));
	}


	// Depth Image
	{

		_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
		VkExtent3D depthImageExtent = { width, height, 1 };
		_depthImage.imageExtent = depthImageExtent;
		VkImageUsageFlags depthImageUsages{};
		depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, depthImageExtent);
		if (USE_MSAA) { dimg_info.samples = MSAA_SAMPLES; }
		VmaAllocationCreateInfo dimg_allocinfo = {};
		dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);// only found on GPU
		vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

		VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image
			, VK_IMAGE_ASPECT_DEPTH_BIT);
		VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));
	}


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

void VulkanEngine::destroy_draw_iamges() {
	vkDestroyImageView(_device, _drawImage.imageView, nullptr);
	vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
	vkDestroyImageView(_device, _depthImage.imageView, nullptr);
	vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
	if (USE_MSAA) {
		vkDestroyImageView(_device, _drawImageBeforeMSAA.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImageBeforeMSAA.image, _drawImageBeforeMSAA.allocation);
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
	std::vector<DescriptorImageData> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &drawImageDescriptor, 1 }
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

	GPUSceneDataMultiDraw multiDrawSceneData;
	multiDrawSceneData.view = view;
	multiDrawSceneData.proj = proj;
	multiDrawSceneData.viewproj = multiDrawSceneData.proj * multiDrawSceneData.view;
	multiDrawSceneData.ambientColor = glm::vec4(.1f);
	multiDrawSceneData.sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.f);
	multiDrawSceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
	multiDrawSceneData.model_count = testMultiDraw.number_of_instances;
	GPUSceneDataMultiDraw* multiDrawSceneUniformData = (GPUSceneDataMultiDraw*)testMultiDraw.sceneDataBuffer.allocation->GetMappedData();
	memcpy(multiDrawSceneUniformData, &multiDrawSceneData, sizeof(GPUSceneDataMultiDraw));


	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time.addValue(elapsed.count() / 1000.f);
}


void VulkanEngine::init_pipelines()
{
	init_compute_pipelines();
	init_fullscreen_pipeline();
	metallicRoughnessPipelines.build_pipelines(this);
	testMultiDraw.build_pipelines(this);

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

void VulkanEngine::init_fullscreen_pipeline()
{
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		_fullscreenDescriptorSetLayout = layoutBuilder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

	}
	_fullscreenDescriptorBuffer = DescriptorBufferSampler(_instance, _device
		, _physicalDevice, _allocator, _fullscreenDescriptorSetLayout, 1);



	VkPipelineLayoutCreateInfo layout_info = vkinit::pipeline_layout_create_info();
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &_fullscreenDescriptorSetLayout;
	layout_info.pPushConstantRanges = nullptr;
	layout_info.pushConstantRangeCount = 0;

	VK_CHECK(vkCreatePipelineLayout(_device, &layout_info, nullptr, &_fullscreenPipelineLayout));




	_fullscreenPipeline = {};

	_fullscreenPipeline.init(_device);
	_fullscreenPipeline.init_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	_fullscreenPipeline.init_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//_fullscreenPipeline.disable_multisampling();
	_fullscreenPipeline.enable_msaa(MSAA_SAMPLES);
	_fullscreenPipeline.init_blending(ShaderObject::BlendMode::NO_BLEND);
	_fullscreenPipeline.disable_depthtesting();


	_fullscreenPipeline._stages[0] = VK_SHADER_STAGE_VERTEX_BIT;
	_fullscreenPipeline._stages[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
	_fullscreenPipeline._stages[2] = VK_SHADER_STAGE_GEOMETRY_BIT;


	vkutil::create_shader_objects(
		"shaders/fullscreen.vert.spv", "shaders/fullscreen.frag.spv"
		, _device, _fullscreenPipeline._shaders
		, 1, &_fullscreenDescriptorSetLayout
		, 0, nullptr
	);


	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, _fullscreenDescriptorSetLayout, nullptr);
		_fullscreenDescriptorBuffer.destroy(_device, _allocator);
		vkDestroyPipelineLayout(_device, _fullscreenPipelineLayout, nullptr);
		PFN_vkDestroyShaderEXT vkDestroyShaderEXT = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(_device, "vkDestroyShaderEXT"));
		vkDestroyShaderEXT(_device, _fullscreenPipeline._shaders[0], nullptr);
		vkDestroyShaderEXT(_device, _fullscreenPipeline._shaders[1], nullptr);
		});
}

void VulkanEngine::init_default_data()
{
#pragma region Basic Textures
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = create_image((void*)&white, 4, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = create_image((void*)&grey, 4, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = create_image((void*)&black, 4, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = create_image(pixels.data(), 16 * 16 * 4, VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
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
		loadedMultiDrawScenes.clear();

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
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(_windowExtent.width / 8.0f)), static_cast<uint32_t>(std::ceil(_windowExtent.height / 8.0f)), 1);


}

void VulkanEngine::draw_fullscreen(VkCommandBuffer cmd, AllocatedImage sourceImage, AllocatedImage targetImage)
{

	VkDescriptorImageInfo fullscreenCombined{};
	fullscreenCombined.sampler = _defaultSamplerNearest;
	fullscreenCombined.imageView = sourceImage.imageView;
	fullscreenCombined.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// needs to match the order of the bindings in the layout
	std::vector<DescriptorImageData> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &fullscreenCombined, 1 }
	};

	_fullscreenDescriptorBuffer.set_data(_device, combined_descriptor, 0);

	VkRenderingAttachmentInfo colorAttachment;
	colorAttachment = vkinit::attachment_info(targetImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &renderInfo);

	_fullscreenPipeline.bind_viewport(cmd, static_cast<float>(_drawExtent.width), static_cast<float>(_drawExtent.height), 0.0f, 1.0f);
	_fullscreenPipeline.bind_scissor(cmd, 0, 0, _drawExtent.width, _drawExtent.height);
	_fullscreenPipeline.bind_input_assembly(cmd);
	_fullscreenPipeline.bind_rasterization(cmd);
	_fullscreenPipeline.bind_depth_test(cmd);
	_fullscreenPipeline.bind_stencil(cmd);
	_fullscreenPipeline.bind_multisampling(cmd);
	_fullscreenPipeline.bind_blending(cmd);
	_fullscreenPipeline.bind_shaders(cmd);
	_fullscreenPipeline.bind_rasterizaer_discard(cmd, VK_FALSE);

	VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info =
		_fullscreenDescriptorBuffer.get_descriptor_buffer_binding_info(_device);
	vkCmdBindDescriptorBuffersEXT(cmd, 1, &descriptor_buffer_binding_info);

	constexpr uint32_t image_buffer_index = 0;
	VkDeviceSize image_buffer_offset = 0;
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _fullscreenPipelineLayout
		, 0, 1, &image_buffer_index, &image_buffer_offset);

	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
	//reset counters
	stats.drawcall_count = 0;
	stats.triangle_count = 0;
	//begin clock
	auto start = std::chrono::system_clock::now();

	VkRenderingAttachmentInfo depthAttachment;
	VkRenderingAttachmentInfo colorAttachment;
	if (USE_MSAA) {
		//VkClearValue colorClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		colorAttachment = vkinit::attachment_info(_drawImageBeforeMSAA.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
		colorAttachment.resolveImageView = _drawImage.imageView;
		colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkClearValue depthClearValue = { 0.0f, 0 };
		depthAttachment = vkinit::attachment_info(_depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
	}
	else {
		colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
		VkClearValue depthClearValue = { 0.0f, 0 };
		depthAttachment = vkinit::attachment_info(_depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	}


	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	auto draw = [&](const RenderObject& draw) {
		if (draw.material != lastMaterial) {
			if (draw.material->pipeline != lastPipeline) {
				lastPipeline = draw.material->pipeline;
				draw.material->pipeline->shaderObject->bind_viewport(cmd, static_cast<float>(_drawExtent.width), static_cast<float>(_drawExtent.height), 0.0f, 1.0f);
				draw.material->pipeline->shaderObject->bind_scissor(cmd, 0, 0, _drawExtent.width, _drawExtent.height);
				draw.material->pipeline->shaderObject->bind_input_assembly(cmd);
				draw.material->pipeline->shaderObject->bind_rasterization(cmd);
				draw.material->pipeline->shaderObject->bind_depth_test(cmd);
				draw.material->pipeline->shaderObject->bind_stencil(cmd);
				draw.material->pipeline->shaderObject->bind_multisampling(cmd);
				draw.material->pipeline->shaderObject->bind_blending(cmd);
				draw.material->pipeline->shaderObject->bind_shaders(cmd);
				draw.material->pipeline->shaderObject->bind_rasterizaer_discard(cmd, VK_FALSE);


				VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[3]{};
				descriptor_buffer_binding_info[0] = gpuSceneDataDescriptorBuffer.get_descriptor_buffer_binding_info(_device);
				descriptor_buffer_binding_info[1] = draw.material->pipeline->materialTextureDescriptorBuffer->get_descriptor_buffer_binding_info(_device);
				descriptor_buffer_binding_info[2] = draw.material->pipeline->materialUniformDescriptorBuffer->get_descriptor_buffer_binding_info(_device);
				vkCmdBindDescriptorBuffersEXT(cmd, 3, descriptor_buffer_binding_info);

				constexpr uint32_t buffer_index_ubo = 0;
				VkDeviceSize global_buffer_offset = 0;
				vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout
					, 0, 1, &buffer_index_ubo, &global_buffer_offset);
			}

			constexpr uint32_t buffer_index_image = 1;
			constexpr uint32_t buffer_index_material = 2;
			VkDeviceSize texture_buffer_offset =
				draw.material->textureDescriptorBufferIndex * draw.material->pipeline->materialTextureDescriptorBuffer->descriptor_buffer_size;
			VkDeviceSize uniform_buffer_offset =
				draw.material->uniformDescriptorBufferIndex * draw.material->pipeline->materialUniformDescriptorBuffer->descriptor_buffer_size;

			vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout
				, 1, 1, &buffer_index_image, &texture_buffer_offset);
			vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout
				, 2, 1, &buffer_index_material, &uniform_buffer_offset);
		}



		if (draw.indexBuffer != lastIndexBuffer) {
			lastIndexBuffer = draw.indexBuffer;
			vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}

		GPUDrawPushConstants pushConstants;
		pushConstants.vertexBuffer = draw.vertexBufferAddress;
		pushConstants.modelMatrix = draw.transform;
		pushConstants.invTransposeModelMatrix = glm::transpose(glm::inverse(glm::mat3(draw.transform)));
		pushConstants.alphaCutoff = draw.material->alphaCutoff;
		vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
		vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);


		//add counters for triangles and draws
		stats.drawcall_count++;
		stats.triangle_count += draw.indexCount / 3;
		};

	std::vector<uint32_t> opaque_draws;
	opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());
	for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
		opaque_draws.push_back(i);
	}
	// sort the opaque surfaces by material and mesh
	std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
		if (A.material == B.material) {
			return A.indexBuffer < B.indexBuffer;
		}
		else {
			return A.material < B.material;
		}
		});

	/*for (auto& r : mainDrawContext.OpaqueSurfaces) {
		draw(r);
	}
	for (auto& r : mainDrawContext.TransparentSurfaces) {
		draw(r);
	}*/

	testMultiDraw.shaderObject->bind_viewport(cmd, static_cast<float>(_drawExtent.width), static_cast<float>(_drawExtent.height), 0.0f, 1.0f);
	testMultiDraw.shaderObject->bind_scissor(cmd, 0, 0, _drawExtent.width, _drawExtent.height);
	testMultiDraw.shaderObject->bind_input_assembly(cmd);
	testMultiDraw.shaderObject->bind_rasterization(cmd);
	testMultiDraw.shaderObject->bind_depth_test(cmd);
	testMultiDraw.shaderObject->bind_stencil(cmd);
	testMultiDraw.shaderObject->bind_multisampling(cmd);
	testMultiDraw.shaderObject->bind_blending(cmd);
	testMultiDraw.shaderObject->bind_shaders(cmd);
	testMultiDraw.shaderObject->bind_rasterizaer_discard(cmd, VK_FALSE);

	VkVertexInputBindingDescription2EXT vertex_description;
	vertex_description.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
	vertex_description.binding = 0;
	vertex_description.pNext = nullptr;
	vertex_description.stride = sizeof(MultiDrawVertex);
	vertex_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertex_description.divisor = 1;

	VkVertexInputAttributeDescription2EXT attribute_descriptions[5];
	attribute_descriptions[0].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
	attribute_descriptions[0].binding = 0;
	attribute_descriptions[0].location = 0;
	attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[0].offset = offsetof(MultiDrawVertex, position);
	attribute_descriptions[1].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
	attribute_descriptions[1].binding = 0;
	attribute_descriptions[1].location = 1;
	attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[1].offset = offsetof(MultiDrawVertex, normal);
	attribute_descriptions[2].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
	attribute_descriptions[2].binding = 0;
	attribute_descriptions[2].location = 2;
	attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribute_descriptions[2].offset = offsetof(MultiDrawVertex, color);
	attribute_descriptions[3].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
	attribute_descriptions[3].binding = 0;
	attribute_descriptions[3].location = 3;
	attribute_descriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
	attribute_descriptions[3].offset = offsetof(MultiDrawVertex, uv);
	attribute_descriptions[4].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
	attribute_descriptions[4].binding = 0;
	attribute_descriptions[4].location = 4;
	attribute_descriptions[4].format = VK_FORMAT_R32_UINT;
	attribute_descriptions[4].offset = offsetof(MultiDrawVertex, materialIndex);


	PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(vkGetDeviceProcAddr(_device, "vkCmdSetVertexInputEXT"));
	vkCmdSetVertexInputEXT(cmd, 1, &vertex_description, 5, attribute_descriptions);


	VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info[2]{};
	descriptor_buffer_binding_info[0] = testMultiDraw.buffer_addresses.get_descriptor_buffer_binding_info(_device);;
	descriptor_buffer_binding_info[1] = testMultiDraw.scene_data.get_descriptor_buffer_binding_info(_device);
	//descriptor_buffer_binding_info[2] = testMultiDraw.texture_data.get_descriptor_buffer_binding_info(_device);
	vkCmdBindDescriptorBuffersEXT(cmd, 2, descriptor_buffer_binding_info);

	constexpr uint32_t buffer_addresses = 0;
	constexpr uint32_t scene_data = 1;
	constexpr uint32_t texture_data = 2;
	VkDeviceSize offsets = 0;
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, testMultiDraw.layout, 0, 1, &buffer_addresses, &offsets);
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, testMultiDraw.layout, 1, 1, &scene_data, &offsets);
	//vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, testMultiDraw.layout, 2, 1, &texture_data, &offsets);

	// specifying offset in the VkDrawIndexedIndirectCommand has no effect if im getting vertices from a BDA! FIX IT!
	VkDeviceSize _offsets[1] = { 0 };
	vkCmdBindIndexBuffer(cmd, testMultiDraw.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindVertexBuffers(cmd, 0, 1, &testMultiDraw.vertexBuffer.buffer, _offsets);
	vkCmdDrawIndexedIndirect(cmd, testMultiDraw.indirectDrawBuffer.buffer, 0, testMultiDraw.number_of_instances, sizeof(VkDrawIndexedIndirectCommand));

	vkCmdEndRendering(cmd);

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.mesh_draw_time.addValue(elapsed.count() / 1000.f);
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

	_drawExtent.height = static_cast<uint32_t>(std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * _renderScale);
	_drawExtent.width = static_cast<uint32_t>(std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * _renderScale);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// draw background onto _drawImage
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	draw_background(cmd);

	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (USE_MSAA) {
		vkutil::transition_image(cmd, _drawImageBeforeMSAA.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		draw_fullscreen(cmd, _drawImage, _drawImageBeforeMSAA);
		//draw_fullscreen(cmd, _drawImageBeforeMSAA, _drawImageBeforeMSAA);
	}
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

		if (resize_requested) { resize_swapchain(); }

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

			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, static_cast<int>(backgroundEffects.size()) - 1);

			ImGui::InputFloat4("data1", (float*)&selected._data.data1);
			ImGui::InputFloat4("data2", (float*)&selected._data.data2);


			ImGui::SliderFloat("Render Scale", &_renderScale, 0.1f, _maxRenderScale);

			ImGui::SliderFloat("Model Scale", &globalModelScale, 0.1f, 20.0f);

			if (ImGui::BeginChild("Stats")) {
				ImGui::Text("frametime %f ms", stats.frametime.getAverage());
				ImGui::Text("draw time %f ms", stats.mesh_draw_time.getAverage());
				ImGui::Text("update time %f ms", stats.scene_update_time.getAverage());
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

		stats.frametime.addValue(elapsed.count() / 1000.f);
	}
}

void VulkanEngine::resize_swapchain() {
	vkDeviceWaitIdle(_device);

	destroy_swapchain();
	destroy_draw_iamges();
	int w, h;
	// get new window size
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	fmt::print("New Screen Resolution: {}x{}\n", w, h);
	// will always be one
	_maxRenderScale = std::min(
		(float)_drawImage.imageExtent.width / (float)_windowExtent.width
		, (float)_drawImage.imageExtent.height / (float)_windowExtent.height
	);
	_maxRenderScale = std::max(_maxRenderScale, 1.0f);
	_maxRenderScale = 1.0f;
	_renderScale = std::min(_maxRenderScale, _renderScale);
	create_swapchain(_windowExtent.width, _windowExtent.height);
	create_draw_images(_windowExtent.width, _windowExtent.height);

	// update compute descrtiptor
	VkDescriptorImageInfo drawImageDescriptor{};
	drawImageDescriptor.imageView = _drawImage.imageView;
	drawImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// needs to match the order of the bindings in the layout
	std::vector<DescriptorImageData> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &drawImageDescriptor, 1 }
	};

	computeImageDescriptorBuffer.set_data(_device, combined_descriptor, 0);



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

AllocatedBuffer VulkanEngine::create_staging_buffer(size_t allocSize)
{
	return create_buffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void VulkanEngine::copy_buffer(AllocatedBuffer src, AllocatedBuffer dst, VkDeviceSize size)
{

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = size;

		vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &vertexCopy);
		});

}

VkDeviceAddress VulkanEngine::get_buffer_address(AllocatedBuffer buffer)
{
	VkBufferDeviceAddressInfo address_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR };
	address_info.buffer = buffer.buffer;
	VkDeviceAddress srcPtr = vkGetBufferDeviceAddress(_device, &address_info);
	return srcPtr;
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



AllocatedImage VulkanEngine::create_image(void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{

	//size_t data_size = size.depth * size.width * size.height * get_channel_count(format);
	size_t data_size = dataSize;
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

int VulkanEngine::get_channel_count(VkFormat format)
{
	switch (format) {
	case VK_FORMAT_R8G8B8A8_UNORM:
		return 4;
	case VK_FORMAT_R8G8B8_UNORM:
		return 3;
	case VK_FORMAT_R8_UNORM:
		return 1;
	default:
		return 0;
	}
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
			, engine->_physicalDevice, engine->_allocator, materialTextureLayout, 200);
		materialUniformDescriptorBuffer = DescriptorBufferUniform(engine->_instance, engine->_device
			, engine->_physicalDevice, engine->_allocator, materialUniformLayout, 200);
	}

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

	opaquePipeline.shaderObject = std::make_shared<ShaderObject>();
	transparentPipeline.shaderObject = std::make_shared<ShaderObject>();
	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;
	pipelineLayout = newLayout;


	opaquePipeline.shaderObject->init(engine->_device);
	opaquePipeline.shaderObject->init_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	opaquePipeline.shaderObject->init_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	if (USE_MSAA) {
		opaquePipeline.shaderObject->enable_msaa(MSAA_SAMPLES);
	}
	else {
		opaquePipeline.shaderObject->disable_multisampling();
	}
	opaquePipeline.shaderObject->init_blending(ShaderObject::BlendMode::NO_BLEND);
	opaquePipeline.shaderObject->enable_depthtesting(true, VK_COMPARE_OP_GREATER_OR_EQUAL);


	opaquePipeline.shaderObject->_stages[0] = VK_SHADER_STAGE_VERTEX_BIT;
	opaquePipeline.shaderObject->_stages[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
	opaquePipeline.shaderObject->_stages[2] = VK_SHADER_STAGE_GEOMETRY_BIT;


	vkutil::create_shader_objects(
		"shaders/mesh.vert.spv", "shaders/mesh.frag.spv"
		, engine->_device, opaquePipeline.shaderObject->_shaders
		, 3, layouts
		, 1, &matrixRange
	);

	// Opaque Pipeline Descriptor Buffer Pointer
	{
		opaquePipeline.materialTextureDescriptorBuffer = &materialTextureDescriptorBuffer;
		opaquePipeline.materialUniformDescriptorBuffer = &materialUniformDescriptorBuffer;
	}

	transparentPipeline.shaderObject->init(engine->_device);
	transparentPipeline.shaderObject->init_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	transparentPipeline.shaderObject->init_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	if (USE_MSAA) {
		transparentPipeline.shaderObject->enable_msaa(MSAA_SAMPLES);
	}
	else {
		transparentPipeline.shaderObject->disable_multisampling();

	}
	transparentPipeline.shaderObject->init_blending(ShaderObject::BlendMode::ADDITIVE_BLEND);
	transparentPipeline.shaderObject->enable_depthtesting(false, VK_COMPARE_OP_GREATER_OR_EQUAL);


	transparentPipeline.shaderObject->_stages[0] = VK_SHADER_STAGE_VERTEX_BIT;
	transparentPipeline.shaderObject->_stages[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
	transparentPipeline.shaderObject->_stages[2] = VK_SHADER_STAGE_GEOMETRY_BIT;

	vkutil::create_shader_objects(
		"shaders/mesh.vert.spv", "shaders/mesh.frag.spv"
		, engine->_device, transparentPipeline.shaderObject->_shaders
		, 3, layouts
		, 1, &matrixRange
	);
	// Transparent Pipeline Descriptor Buffer Creation
	{
		transparentPipeline.materialTextureDescriptorBuffer = &materialTextureDescriptorBuffer;
		transparentPipeline.materialUniformDescriptorBuffer = &materialUniformDescriptorBuffer;
	}
}

MaterialInstance GLTFMetallic_Roughness::write_material(
	VkDevice device
	, MaterialPass pass
	, const MaterialResources& resources
) {
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
	std::vector<DescriptorImageData> combined_descriptor = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, &colorCombinedDescriptor, 1 }
		, { VK_DESCRIPTOR_TYPE_SAMPLER, &metalRoughCombinedDescriptor, 1 }
		, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &colorCombinedDescriptor, 1 }
		, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &metalRoughCombinedDescriptor, 1 }
	};

	matData.colorDescriptorImageInfo = colorCombinedDescriptor;
	matData.metalRoughDescriptorImageInfo = metalRoughCombinedDescriptor;
	matData.materialUniformBuffer = resources.dataBuffer;


	matData.textureDescriptorBufferIndex = matData.pipeline->materialTextureDescriptorBuffer->setup_data(
		device, combined_descriptor
	);
	matData.uniformDescriptorBufferIndex = matData.pipeline->materialUniformDescriptorBuffer->setup_data(
		device, resources.dataBuffer, resources.dataBufferSize
	);

	matData.alphaCutoff = resources.alphaCutoff;

	return matData;
}


void GLTFMetallic_Roughness::destroy(VkDevice device, VmaAllocator allocator)
{
	vkDestroyDescriptorSetLayout(device, materialTextureLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, materialUniformLayout, nullptr);
	materialTextureDescriptorBuffer.destroy(device, allocator);
	materialUniformDescriptorBuffer.destroy(device, allocator);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	PFN_vkDestroyShaderEXT vkDestroyShaderEXT = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(device, "vkDestroyShaderEXT"));
	vkDestroyShaderEXT(device, opaquePipeline.shaderObject->_shaders[0], nullptr);
	vkDestroyShaderEXT(device, opaquePipeline.shaderObject->_shaders[1], nullptr);
	vkDestroyShaderEXT(device, transparentPipeline.shaderObject->_shaders[0], nullptr);
	vkDestroyShaderEXT(device, transparentPipeline.shaderObject->_shaders[1], nullptr);
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

void GLTFMetallic_RoughnessMultiDraw::build_pipelines(VulkanEngine* engine)
{
	// Defining Descriptor Layouts
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		bufferAddressesDescriptorSetLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	}
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		sceneDataDescriptorSetLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

	}
	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_SAMPLER, 32); // I dont expect any models to have more than 32 samplers
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 255); // 255 is upper limit of textures

		textureDescriptorSetLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	}

	buffer_addresses = DescriptorBufferUniform(engine->_instance, engine->_device
		, engine->_physicalDevice, engine->_allocator, bufferAddressesDescriptorSetLayout, 1);
	scene_data = DescriptorBufferUniform(engine->_instance, engine->_device
		, engine->_physicalDevice, engine->_allocator, sceneDataDescriptorSetLayout, 1);
	texture_data = DescriptorBufferSampler(engine->_instance, engine->_device
		, engine->_physicalDevice, engine->_allocator, textureDescriptorSetLayout, 2);

	VkDescriptorSetLayout layouts[] = {
		bufferAddressesDescriptorSetLayout,
		sceneDataDescriptorSetLayout,
		textureDescriptorSetLayout,
	};

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 3;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = nullptr;
	mesh_layout_info.pushConstantRangeCount = 0;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

	shaderObject = std::make_shared<ShaderObject>();
	layout = newLayout;


	shaderObject->init(engine->_device);
	shaderObject->init_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	shaderObject->init_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	if (USE_MSAA) {
		shaderObject->enable_msaa(MSAA_SAMPLES);
	}
	else {
		shaderObject->disable_multisampling();
	}
	shaderObject->init_blending(ShaderObject::BlendMode::NO_BLEND);
	shaderObject->enable_depthtesting(true, VK_COMPARE_OP_GREATER_OR_EQUAL);


	shaderObject->_stages[0] = VK_SHADER_STAGE_VERTEX_BIT;
	shaderObject->_stages[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderObject->_stages[2] = VK_SHADER_STAGE_GEOMETRY_BIT;


	vkutil::create_shader_objects(
		"shaders/meshIndirect.vert.spv", "shaders/meshIndirect.frag.spv"
		, engine->_device, shaderObject->_shaders
		, 3, layouts
		, 0, nullptr
	);

	engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(engine->_device, bufferAddressesDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, sceneDataDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, textureDescriptorSetLayout, nullptr);
		buffer_addresses.destroy(engine->_device, engine->_allocator);
		scene_data.destroy(engine->_device, engine->_allocator);
		texture_data.destroy(engine->_device, engine->_allocator);
		vkDestroyPipelineLayout(engine->_device, layout, nullptr);
		PFN_vkDestroyShaderEXT vkDestroyShaderEXT = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(engine->_device, "vkDestroyShaderEXT"));
		vkDestroyShaderEXT(engine->_device, shaderObject->_shaders[0], nullptr);
		vkDestroyShaderEXT(engine->_device, shaderObject->_shaders[1], nullptr);
		});
}

void GLTFMetallic_RoughnessMultiDraw::build_buffers(VulkanEngine* engine, LoadedGLTFMultiDraw& scene)
{
	if (buffersBuilt) { return; }
	buffersBuilt = true;

	// create renderables from the scenenodes
	glm::mat4 mMatrix = glm::mat4(1.0f);

	for (auto& n : scene.topNodes) {
		recursive_node_process(scene, *n.get(), mMatrix);
	}
	number_of_instances = instanceData.size();

	vertexBuffer = engine->create_buffer(vertex_buffer_size
		, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
		, VMA_MEMORY_USAGE_GPU_ONLY);
	indexBuffer = engine->create_buffer(index_buffer_size
		, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
		, VMA_MEMORY_USAGE_GPU_ONLY);
	instanceBuffer = engine->create_buffer(instanceData.size() * sizeof(InstanceData)
		, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		, VMA_MEMORY_USAGE_GPU_ONLY);
	materialBuffer = engine->create_buffer(scene.materials.size() * sizeof(MaterialData)
		, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		, VMA_MEMORY_USAGE_GPU_ONLY);



	AllocatedBuffer staging_vertex = engine->create_staging_buffer(vertex_buffer_size);
	AllocatedBuffer staging_index = engine->create_staging_buffer(index_buffer_size);
	AllocatedBuffer staging_instance = engine->create_staging_buffer(instanceData.size() * sizeof(InstanceData));
	AllocatedBuffer staging_material = engine->create_staging_buffer(meshData.size() * sizeof(MaterialData));

	for (size_t i = 0; i < number_of_instances; i++) {
		MeshData& d = meshData[i];
		memcpy(
			(char*)staging_vertex.info.pMappedData + d.vertex_buffer_offset
			, d.vertices.data()
			, d.vertices.size() * sizeof(MultiDrawVertex));
		memcpy(
			(char*)staging_index.info.pMappedData + d.index_buffer_offset
			, d.indices.data()
			, d.indices.size() * sizeof(uint32_t));
		memcpy(
			(char*)staging_instance.info.pMappedData + i * sizeof(InstanceData)
			, &instanceData[i]
			, sizeof(InstanceData));
	}
	memcpy(staging_material.info.pMappedData, scene.materials.data(), scene.materials.size() * sizeof(MaterialData));

	engine->copy_buffer(staging_vertex, vertexBuffer, vertex_buffer_size);
	engine->copy_buffer(staging_index, indexBuffer, index_buffer_size);
	engine->copy_buffer(staging_instance, instanceBuffer, instanceData.size() * sizeof(InstanceData));
	engine->copy_buffer(staging_material, materialBuffer, scene.materials.size() * sizeof(MaterialData));
	engine->destroy_buffer(staging_index);
	engine->destroy_buffer(staging_vertex);
	engine->destroy_buffer(staging_instance);
	engine->destroy_buffer(staging_material);




	constexpr auto default_indirect_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	auto           indirect_flags = default_indirect_flags;
	indirect_flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	indirectDrawBuffer = engine->create_buffer(number_of_instances * sizeof(VkDrawIndexedIndirectCommand)
		, indirect_flags, VMA_MEMORY_USAGE_GPU_ONLY);
	// seed data
	std::vector<VkDrawIndexedIndirectCommand> cpu_commands;
	cpu_commands.resize(number_of_instances);
	for (size_t i = 0; i < number_of_instances; ++i) {
		VkDrawIndexedIndirectCommand cmd{};
		cmd.firstIndex = meshData[i].index_buffer_offset / (sizeof(meshData[i].indices[0]));
		cmd.indexCount = static_cast<uint32_t>(meshData[i].indices.size());
		cmd.vertexOffset = static_cast<int32_t>(meshData[i].vertex_buffer_offset / sizeof(MultiDrawVertex));
		cmd.firstInstance = i;
		cmd.instanceCount = 1;
		cpu_commands[i] = cmd;
	}

	AllocatedBuffer staging_indirect = engine->create_staging_buffer(number_of_instances * sizeof(VkDrawIndexedIndirectCommand));
	memcpy(staging_indirect.info.pMappedData, cpu_commands.data(), number_of_instances * sizeof(VkDrawIndexedIndirectCommand));
	engine->copy_buffer(staging_indirect, indirectDrawBuffer, number_of_instances * sizeof(VkDrawIndexedIndirectCommand));



	VkDeviceAddress addresses[3];
	addresses[0] = engine->get_buffer_address(vertexBuffer);
	addresses[1] = engine->get_buffer_address(materialBuffer);
	addresses[2] = engine->get_buffer_address(instanceBuffer);

	buffer_addresses_underlying = engine->create_buffer(sizeof(addresses), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	AllocatedBuffer staging_addresses = engine->create_staging_buffer(sizeof(addresses));
	memcpy(staging_addresses.info.pMappedData, addresses, sizeof(addresses));
	engine->copy_buffer(staging_addresses, buffer_addresses_underlying, sizeof(addresses));
	engine->destroy_buffer(staging_addresses);
	buffer_addresses.setup_data(engine->_device, buffer_addresses_underlying, sizeof(addresses));


	std::vector<VkDescriptorImageInfo> textureDescriptors;
	for (int i = 0; i < scene.images.size(); i++) { 
		textureDescriptors.push_back(
			{.imageView = scene.images[i].imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
	};

	std::vector<VkDescriptorImageInfo> samplerDescriptors;
	for (int i = 0; i < scene.samplers.size(); i++) { samplerDescriptors.push_back( { .sampler = scene.samplers[i] } ); };

	std::vector<DescriptorImageData> texture_descriptors;
	texture_descriptors.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, samplerDescriptors.data(), scene.samplers.size() });
	texture_descriptors.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureDescriptors.data() , scene.images.size() });
	texture_data.setup_data(engine->_device, texture_descriptors);


	sceneDataBuffer = engine->create_buffer(sizeof(GPUSceneDataMultiDraw), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	scene_data.setup_data(engine->_device, sceneDataBuffer, sizeof(GPUSceneDataMultiDraw));

}

void GLTFMetallic_RoughnessMultiDraw::recursive_node_process(LoadedGLTFMultiDraw& scene, Node& node, glm::mat4& topMatrix)
{
	if (MeshNodeMultiDraw* meshNode = dynamic_cast<MeshNodeMultiDraw*>(&node)) {
		glm::mat4 nodeMatrix = topMatrix * meshNode->worldTransform;

		MeshData mdata;
		mdata.vertex_buffer_offset = vertex_buffer_size;
		mdata.index_buffer_offset = index_buffer_size;

		PrimitiveData& d = scene.primitives[meshNode->meshIndex];
		mdata.vertices = d.vertices;
		mdata.indices = d.indices;

		vertex_buffer_size += d.vertices.size() * sizeof(MultiDrawVertex);
		index_buffer_size += d.indices.size() * sizeof(d.indices[0]);
		assert(d.indices.size() % 3 == 0);

		instanceData.push_back({ nodeMatrix });
		meshData.push_back(mdata);
	}

	for (auto& child : node.children) {
		recursive_node_process(scene, *child, topMatrix);
	}

}

void MeshNodeMultiDraw::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	fmt::print("DO NOT CALL THIS ON MESHNODEMULTIDRAW\n");
	abort();
}
