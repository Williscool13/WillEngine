// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_descriptor_buffer.h>

constexpr unsigned int FRAME_OVERLAP = 2;

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


struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants _data;
};

struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	// Frame Lifetime Deletion Queue
	DeletionQueue _deletionQueue;
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};



struct GLTFMetallic_Roughness {
	// 2x pipelines
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;
	VkPipelineLayout pipelineLayout;

	bool pipeline_layout_initialized = false;
	VkDescriptorSetLayout materialTextureLayout;
	VkDescriptorSetLayout materialUniformLayout;
	DescriptorBufferSampler materialTextureDescriptorBuffer;
	DescriptorBufferUniform materialUniformDescriptorBuffer;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		AllocatedBuffer dataBuffer;
		uint32_t dataBufferSize;
		uint32_t dataBufferOffset;
	};

	//DescriptorWriter writer;
	


	void build_pipelines(VulkanEngine* engine);

	

	MaterialInstance write_material(
		VkDevice device
		, MaterialPass pass
		, const MaterialResources& resources
	);


	void destroy(VkDevice device, VmaAllocator allocator);
};





class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	bool resize_requested{ false };

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkSurfaceKHR _surface;

	VkDevice _oldDevice;

	// Global Lifetime Deletion Queue
	DeletionQueue _mainDeletionQueue;

	// Memory allocator
	VmaAllocator _allocator;

	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;

	// Render Scale
	float _renderScale{ 1.0f };
	float _maxRenderScale{ 1.0f };

	// Swapchain
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	// Graphics Queue Family
	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	// Descriptor, Layout, Allocator
	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptorSet;
	VkDescriptorSetLayout _drawImageDescriptorLayout;


	// Background (Compute)
	VkPipelineLayout _backgroundEffectPipelineLayout;
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	// Mesh (Graphics)
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	GPUMeshBuffers rectangle;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;


	void init();
	void cleanup();
	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void draw();
	void run();

	void resize_swapchain();

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	void destroy_buffer(const AllocatedBuffer& buffer);


	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	// Descriptor Buffer vars
	VkDescriptorSetLayout textureDescriptorBufferSetLayout;
	DescriptorBufferSampler textureDescriptorBuffer;
	VkDescriptorSetLayout gpuSceneDataDescriptorBufferSetLayout;
	DescriptorBufferUniform gpuSceneDataDescriptorBuffer;
	AllocatedBuffer gpuSceneDataBuffer;
	
	// Textures
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);

	// Material Pipeline
	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;
	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	void update_scene();

private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_dearimgui();
	void init_descriptor_buffer();
	void init_descriptor_buffer_data();
	void init_scene_data_descriptor_buffer();

	void init_pipelines();
	void init_compute_pipelines();
	void init_mesh_pipeline();

	void init_data();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
};





