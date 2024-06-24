#include "vk_engine.h"

// defined here because needs implementation in translation unit
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image_write.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#ifdef NDEBUG
#define USE_VALIDATION_LAYERS false
#else
#define USE_VALIDATION_LAYERS true
#endif

#define ENABLE_FRAME_STATISTICS true
#define USE_MSAA true
#define MSAA_SAMPLES VK_SAMPLE_COUNT_4_BIT



VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

glm::mat4 modelMatrix{ 1.0f };

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

	fmt::print("==========================================================================================\n");

	init_swapchain();

	init_commands();

	init_sync_structures();

	_resourceConstructor = std::make_unique<VulkanResourceConstructor>(this);
	_environmentMap = std::make_shared<EnvironmentMap>(this, EnvironmentMap::defaultEquiPath);
	fmt::print(". . .\n");

	init_default_data();

	init_descriptors();

	init_pipelines();

	init_dearimgui();

	//std::string structurePath = { "assets\\models\\structure.glb" };
	//mainCamera.position = glm::vec3(30.f, -00.f, -085.f);
	//mainCamera.yaw = -90.0f;

	std::string structurePath = { "assets\\models\\MetalRoughSpheres\\glTF\\MetalRoughSpheres.gltf" };

	//std::string structurePath = { "assets\\models\\primitives\\primitives.gltf" };   
	//std::string structurePath = { "assets\\models\\vokselia\\vokselia.gltf" };
	//std::string structurePath = { "assets\\models\\virtual_city\\VirtualCity.glb" };
	//std::string structurePath = { "assets\\models\\AlphaBlendModeTest\\glTF-Binary\\AlphaBlendModeTest.glb" };

	multiDrawPipeline = std::make_shared<GLTFMetallic_RoughnessMultiDraw>(
		this, structurePath
		, "shaders/pbr.vert.spv", "shaders/pbr.frag.spv"
		, USE_MSAA, MSAA_SAMPLES
	);
	fmt::print(". . .\n");

	_isInitialized = true;
	fmt::print("Finished Initialization\n");
	fmt::print("==========================================================================================\n");
}

// Output: Instance, Physical Device, Device, Surface, Graphics Queue, Graphics Queue Family, Allocator
void VulkanEngine::init_vulkan()
{
	// volk init
	VkResult res = volkInitialize();
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Failed to initialize volk");
	}
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

	// volk init
	volkLoadInstance(_instance);

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


	// Graphics Queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	// VMA
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _physicalDevice;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;
	vmaCreateAllocator(&allocatorInfo, &_allocator);
}

#pragma region Swapchain / Draw Image
void VulkanEngine::init_swapchain()
{
	create_swapchain(_windowExtent.width, _windowExtent.height);
	create_draw_images(_windowExtent.width, _windowExtent.height);
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
}
#pragma endregion

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
}

void VulkanEngine::init_descriptors()
{

	{
		DescriptorLayoutBuilder layoutBuilder;
		layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		singleUniformDescriptorSetLayout = layoutBuilder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
			, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

	}

	_sceneDataBuffer = _resourceConstructor->create_buffer(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	_sceneDataDescriptorBuffer = DescriptorBufferUniform(_instance, _device, _physicalDevice, _allocator, singleUniformDescriptorSetLayout, 1);
	_sceneDataDescriptorBuffer.setup_data(_device, _sceneDataBuffer, sizeof(SceneData));


	// View matrix is different and only needs view/proj/viewproj
	_environmentMapSceneDataBuffer = _resourceConstructor->create_buffer(sizeof(CubemapSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	_environmentMapSceneDataDescriptorBuffer = DescriptorBufferUniform(_instance, _device
		, _physicalDevice, _allocator, singleUniformDescriptorSetLayout, 1);
	_environmentMapSceneDataDescriptorBuffer.setup_data(_device, _environmentMapSceneDataBuffer, sizeof(CubemapSceneData));

	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, singleUniformDescriptorSetLayout, nullptr);
		_resourceConstructor->destroy_buffer(_sceneDataBuffer);
		_sceneDataDescriptorBuffer.destroy(_device, _allocator);

		_resourceConstructor->destroy_buffer(_environmentMapSceneDataBuffer);
		_environmentMapSceneDataDescriptorBuffer.destroy(_device, _allocator);
		});
}

void VulkanEngine::update_scene()
{
	auto start = std::chrono::system_clock::now();

	mainCamera.update();
	glm::mat4 view = mainCamera.getViewMatrix();
	//glm::mat4 view = glm::lookAt(glm::vec3(0, 0, camera_dist), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 proj = glm::perspective(glm::radians(70.0f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.0f, 0.1f);
	proj[1][1] *= -1;

	SceneData sceneData{};
	sceneData.view = view;
	sceneData.proj = proj;
	sceneData.viewproj = sceneData.proj * sceneData.view;
	sceneData.ambientColor = glm::vec4(.1f);
	sceneData.sunlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 2.0f);
	sceneData.sunlightDirection = glm::vec4(0, 1, 0.5f, 1.f); // inverted to match openGL up/down
	sceneData.cameraPosition = glm::vec4(mainCamera.position, 1.f);

	SceneData* p_scene_data = (SceneData*)_sceneDataBuffer.info.pMappedData;
	memcpy(p_scene_data, &sceneData, sizeof(SceneData));

	CubemapSceneData environmentMapSceneData{};
	glm::vec3 camera_view_direction = mainCamera.getViewDirection();

	environmentMapSceneData.view = glm::lookAt(glm::vec3(0), camera_view_direction, glm::vec3(0, 1, 0));
	proj[1][1] *= -1;
	environmentMapSceneData.proj = proj;
	environmentMapSceneData.viewproj = environmentMapSceneData.proj * environmentMapSceneData.view;

	CubemapSceneData* p_cubemap_scene_data = (CubemapSceneData*)_environmentMapSceneDataBuffer.info.pMappedData;
	memcpy(p_cubemap_scene_data, &environmentMapSceneData, sizeof(CubemapSceneData));


	multiDrawPipeline->update_model_matrix(modelMatrix);

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.scene_update_time.addValue(elapsed.count() / 1000.f);
}


void VulkanEngine::init_pipelines()
{
	//  Environment Map Background
	{
		assert(EnvironmentMap::layoutsCreated);
		VkDescriptorSetLayout layouts[2] =
		{ singleUniformDescriptorSetLayout, EnvironmentMap::_cubemapDescriptorSetLayout };

		VkPipelineLayoutCreateInfo layout_info = vkinit::pipeline_layout_create_info();
		layout_info.setLayoutCount = 2;
		layout_info.pSetLayouts = layouts;
		layout_info.pPushConstantRanges = nullptr;
		layout_info.pushConstantRangeCount = 0;

		VK_CHECK(vkCreatePipelineLayout(_device, &layout_info, nullptr, &_environmentPipelineLayout));

		_environmentPipeline = {};

		_environmentPipeline.init_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		_environmentPipeline.init_rasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		if (USE_MSAA) _environmentPipeline.enable_msaa(MSAA_SAMPLES);
		else _environmentPipeline.disable_multisampling();

		_environmentPipeline.init_blending(ShaderObject::BlendMode::NO_BLEND);
		_environmentPipeline.enable_depthtesting(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

		_environmentPipeline._stages[0] = VK_SHADER_STAGE_VERTEX_BIT;
		_environmentPipeline._stages[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
		_environmentPipeline._stages[2] = VK_SHADER_STAGE_GEOMETRY_BIT;

		vkutil::create_shader_objects(
			"shaders/environment/environment.vert.spv", "shaders/environment/environment.frag.spv"
			, _device, _environmentPipeline._shaders
			, 2, layouts
			, 0, nullptr
		);

		_mainDeletionQueue.push_function([=]() {
			vkDestroyPipelineLayout(_device, _environmentPipelineLayout, nullptr);
			vkDestroyShaderEXT(_device, _environmentPipeline._shaders[0], nullptr);
			vkDestroyShaderEXT(_device, _environmentPipeline._shaders[1], nullptr);
			});
	}
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

void VulkanEngine::init_default_data()
{
#pragma region Basic Textures
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = _resourceConstructor->create_image((void*)&white, 4, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = _resourceConstructor->create_image((void*)&grey, 4, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = _resourceConstructor->create_image((void*)&black, 4, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels{}; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = _resourceConstructor->create_image(pixels.data(), 16 * 16 * 4, VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
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
		_resourceConstructor->destroy_image(_whiteImage);
		_resourceConstructor->destroy_image(_greyImage);
		_resourceConstructor->destroy_image(_blackImage);
		_resourceConstructor->destroy_image(_errorCheckerboardImage);
		vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
		vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
		});
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


		multiDrawPipeline.reset();
		_environmentMap.reset();

		_mainDeletionQueue.flush();

		for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
		}

		vkDestroyCommandPool(_device, _immCommandPool, nullptr);
		vkDestroyFence(_device, _immFence, nullptr);

		destroy_draw_iamges();
		destroy_swapchain();

		vmaDestroyAllocator(_allocator);

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);

		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}

	// clear engine pointer
	loadedEngine = nullptr;
}

void VulkanEngine::cull_geometry(VkCommandBuffer cmd)
{
	// Frustum Culling
	multiDrawPipeline->cull(cmd);

	// Barriers
	if (multiDrawPipeline->hasTransparents()) {
		VkBufferMemoryBarrier barrier;
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.pNext = nullptr;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.offset = 0;
		barrier.buffer = multiDrawPipeline->transparentDrawBuffers.indirectDrawBuffer.buffer;
		barrier.size = VK_WHOLE_SIZE;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	}
	if (multiDrawPipeline->hasOpaques()) {
		VkBufferMemoryBarrier barrier2;
		barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier2.pNext = nullptr;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.offset = 0;
		barrier2.size = VK_WHOLE_SIZE;
		barrier2.buffer = multiDrawPipeline->opaqueDrawBuffers.indirectDrawBuffer.buffer;
		barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;


		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &barrier2, 0, nullptr);
	}
}


void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
	auto start = std::chrono::system_clock::now();

	stats.drawcall_count = 0;
	stats.vertex_count = 0;
	stats.triangle_count = 0;

	multiDrawPipeline->draw(cmd, _drawExtent);

	if (ENABLE_FRAME_STATISTICS) {
		for (int i = 0; i < multiDrawPipeline->instanceData.size(); i++) {
			stats.triangle_count += multiDrawPipeline->instanceData[i].indexCount / 3;
			stats.drawcall_count++;
		}
	}


	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	stats.mesh_draw_time.addValue(elapsed.count() / 1000.f);
}

void VulkanEngine::draw_environment(VkCommandBuffer cmd)
{
	_environmentPipeline.bind_viewport(cmd, static_cast<float>(_drawExtent.width), static_cast<float>(_drawExtent.height), 0.0f, 1.0f);
	_environmentPipeline.bind_scissor(cmd, 0, 0, _drawExtent.width, _drawExtent.height);
	_environmentPipeline.bind_input_assembly(cmd);
	_environmentPipeline.bind_rasterization(cmd);
	_environmentPipeline.bind_depth_test(cmd);
	_environmentPipeline.bind_stencil(cmd);
	_environmentPipeline.bind_multisampling(cmd);
	_environmentPipeline.bind_blending(cmd);
	_environmentPipeline.bind_shaders(cmd);
	_environmentPipeline.bind_rasterizaer_discard(cmd, VK_FALSE);

	VkDescriptorBufferBindingInfoEXT bindings[2] = {
		_environmentMapSceneDataDescriptorBuffer.get_descriptor_buffer_binding_info(),
		_environmentMap->get_cubemap_descriptor_buffer().get_descriptor_buffer_binding_info(),
		//_cubemapDescriptorBuffer.get_descriptor_buffer_binding_info(),
	};

	vkCmdBindDescriptorBuffersEXT(cmd, 2, bindings);

	constexpr uint32_t scene_data_index = 0;
	constexpr uint32_t image_buffer_index = 1;

	VkDeviceSize buffer_offset = 0;
	VkDeviceSize image_buffer_offset = 0;//_environmentMap->get_cubemap_descriptor_buffer().descriptor_buffer_size;
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _environmentPipelineLayout
		, 0, 1, &scene_data_index, &buffer_offset);
	vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _environmentPipelineLayout
		, 1, 1, &image_buffer_index, &image_buffer_offset);

	vkCmdDraw(cmd, 3, 1, 0, 0);
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

	//cull_geometry(cmd);

	// draw background onto _drawImage
	//vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	//draw_background(cmd);

	//vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	//if (USE_MSAA) {
	//	vkutil::transition_image(cmd, _drawImageBeforeMSAA.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	//	draw_fullscreen(cmd, _drawImage, _drawImageBeforeMSAA);
	//}
	if (USE_MSAA) {
		vkutil::transition_image(cmd, _drawImageBeforeMSAA.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingAttachmentInfo depthAttachment;
	VkRenderingAttachmentInfo colorAttachment;
	if (USE_MSAA) {
		VkClearValue colorClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		colorAttachment = vkinit::attachment_info(_drawImageBeforeMSAA.imageView, &colorClearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
		colorAttachment.resolveImageView = _drawImage.imageView;
		colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkClearValue depthClearValue = { 0.0f, 0 };
		depthAttachment = vkinit::attachment_info(_depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
	}
	else {
		VkClearValue colorClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		colorAttachment = vkinit::attachment_info(_drawImage.imageView, &colorClearValue, VK_IMAGE_LAYOUT_GENERAL);
		VkClearValue depthClearValue = { 0.0f, 0 };
		depthAttachment = vkinit::attachment_info(_depthImage.imageView, &depthClearValue, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	}
	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	draw_environment(cmd);
	draw_geometry(cmd);

	vkCmdEndRendering(cmd);

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
		bool show_demo_window = true;
		if (ImGui::Begin("background")) {
			ImGui::SliderFloat("Render Scale", &_renderScale, 0.1f, _maxRenderScale);

			ImGui::SliderFloat("Model Scale", &globalModelScale, 0.1f, 20.0f);


			if (ImGui::Button("Change Model Matrix")) {
				modelMatrix = glm::translate(modelMatrix, glm::vec3(0, 0, 1));
			}

			if (ImGui::BeginChild("Stats")) {
				ImGui::Text("frametime %f ms", stats.frametime.getAverage());
				ImGui::Text("draw time %f ms", stats.mesh_draw_time.getAverage());
				ImGui::Text("update time %f ms", stats.scene_update_time.getAverage());
				ImGui::Text("triangles %i", stats.triangle_count);
				ImGui::Text("vertices %i", stats.vertex_count);
				ImGui::Text("draws %i", stats.drawcall_count);
			}
			ImGui::EndChild();


		}
		ImGui::End();
		ImGui::Render();

		draw();


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

	resize_requested = false;
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