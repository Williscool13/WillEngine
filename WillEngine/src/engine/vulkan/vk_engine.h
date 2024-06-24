// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include "will_engine.h"

#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_descriptor_buffer.h>
#include <vk_initializers.h>
#include <vk_images.h>
#include <vk_pipelines.h>
#include "vk_draw_structure.h"
#include <vk_constructors.h>
#include <vk_environment_map.h>

#include <camera.h>


constexpr unsigned int FRAME_OVERLAP = 2;

struct LoadedGLTFMultiDraw;
struct GLTFMetallic_RoughnessMultiDraw;
class VulkanResourceConstructor;
class EnvironmentMap;

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};


struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	// Frame Lifetime Deletion Queue
	DeletionQueue _deletionQueue;
};

struct EngineStats {

	int triangle_count;
	int vertex_count;
	int drawcall_count;
	RollingAverage frametime{ 500 };
	RollingAverage scene_update_time{ 500 };
	RollingAverage mesh_draw_time{ 500 };
};



class VulkanEngine {
public:


	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool stop_rendering{ false };
	bool resize_requested{ false };

	VkExtent2D _windowExtent{ 1700 , 900 };
	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	Camera mainCamera;
	bool mouseLocked{ true };
	EngineStats stats;

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkSurfaceKHR _surface;
	VmaAllocator _allocator;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	std::shared_ptr<VulkanResourceConstructor> _resourceConstructor;

	// Swapchain
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);


	// Global Lifetime Deletion Queue
	DeletionQueue _mainDeletionQueue;

	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _drawImageBeforeMSAA;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;
	float _renderScale{ 1.0f };
	float _maxRenderScale{ 1.0f };


	void init();
	void cleanup();
	void draw();
	void run();


	void resize_swapchain();

	// Textures
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	// Environment Map
	std::shared_ptr<EnvironmentMap> _environmentMap;
	int _currentEnvironmentMapIndex{ 1 };

	// Material Pipeline
	std::shared_ptr<GLTFMetallic_RoughnessMultiDraw> multiDrawPipeline;

	float globalModelScale{ 1.0f };
	void update_scene();


	VkDescriptorSetLayout singleUniformDescriptorSetLayout;
	AllocatedBuffer _sceneDataBuffer;
	DescriptorBufferUniform _sceneDataDescriptorBuffer;
	AllocatedBuffer _environmentMapSceneDataBuffer;
	DescriptorBufferUniform _environmentMapSceneDataDescriptorBuffer;

	VkPipelineLayout _environmentPipelineLayout;
	ShaderObject _environmentPipeline;


	// getters
	VkDescriptorSetLayout get_scene_data_descriptor_set_layout() const { return singleUniformDescriptorSetLayout; }
	DescriptorBufferUniform get_scene_data_descriptor_buffer() const { return _sceneDataDescriptorBuffer; }

	EnvironmentMap* get_current_environment_map() { return _environmentMap.get(); }
	int get_current_environment_map_index() { return _currentEnvironmentMapIndex; }
private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_dearimgui();

	void init_pipelines();
	void init_default_data();

	void create_swapchain(uint32_t width, uint32_t height);
	void create_draw_images(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void destroy_draw_iamges();


	void cull_geometry(VkCommandBuffer cmd);

	void draw_geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void draw_environment(VkCommandBuffer cmd);
};