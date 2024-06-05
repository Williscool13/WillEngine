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
#include <camera.h>

constexpr unsigned int FRAME_OVERLAP = 2;

struct MeshAsset;
struct LoadedGLTF;
struct LoadedGLTFMultiDraw;

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

struct EngineStats {
	int triangle_count;
	int drawcall_count;
	RollingAverage frametime{ 500 };
	RollingAverage scene_update_time{ 500 };
	RollingAverage mesh_draw_time{ 500 };
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct MeshNodeMultiDraw : public Node {
	
	uint32_t meshIndex;
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
	std::vector<RenderObject> TransparentSurfaces;
};

class VulkanEngine;

struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;	
	VkPipelineLayout pipelineLayout;

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
		float alphaCutoff;
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



struct GPUSceneDataMultiDraw {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
	uint32_t model_count;
};

struct MultiDrawIndirectBuffers {
	AllocatedBuffer indirectDrawBuffer;
};

struct GLTFMetallic_RoughnessMultiDraw {
	VulkanEngine* engine;

	std::shared_ptr<ShaderObject> shaderObject;
	// holds indirect draw buffers. Transparent will not go through compute culling.

	VkPipelineLayout layout;
	VkDescriptorSetLayout bufferAddressesDescriptorSetLayout;
	VkDescriptorSetLayout sceneDataDescriptorSetLayout;
	VkDescriptorSetLayout textureDescriptorSetLayout;

	AllocatedBuffer indexBuffer;

	// BINDING 0: BUFFER ADDRESSES
	DescriptorBufferUniform buffer_addresses;
	AllocatedBuffer buffer_addresses_underlying;
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer materialBuffer;
	AllocatedBuffer instanceBuffer; 
	// access correct instance data w/ gl_instanceID. 
	//  IMPORTANT: WHEN DEFINING VkDrawIndexedIndirectCommand, MAKE SURE FIRST_INSTANCE IS THE INDEX OF THE INSTANCE DATA
	
	// BINDING 1: SCENE DATA
	DescriptorBufferUniform scene_data;
	AllocatedBuffer sceneDataBuffer;
	// BINDING 2: TEXTURE DATA
	//	when initializing, set descriptor count to be equal to the number of textures
	DescriptorBufferUniform texture_data;
	AllocatedBuffer textureBuffer;
	AllocatedBuffer samplerBuffer;


	// BINDING 3: COMPUTE CULLING DATA
	//  MAKE IT

	// BINDING 4: INDIRECT DRAW BUFFER
	DescriptorBufferUniform indirect_draw_buffer_address;
	AllocatedBuffer indirect_draw_buffer_underlying;
	AllocatedBuffer indirectDrawBuffer;


	MultiDrawIndirectBuffers opaqueDrawBuffer;
	MultiDrawIndirectBuffers transparentDrawBuffer;

	void build_pipelines(VulkanEngine* engine);


	// buffer building
	size_t vertex_buffer_size = 0;
	size_t index_buffer_size = 0;
	size_t number_of_instances = 0;
	std::vector<InstanceData> instanceData;
	std::vector<MeshData> meshData;
	void build_buffers(VulkanEngine* engine, LoadedGLTFMultiDraw& scene);
	void recursive_node_process(LoadedGLTFMultiDraw& scene, Node& node, glm::mat4& topMatrix);

	bool buffersBuilt{ false };
};



//struct MultiDrawIndirect {
//	std::vector<SceneModel> models;
//
//	AllocatedBuffer combinedVertexBuffer;
//	AllocatedBuffer combinedIndexBuffer;
//	// to calculate culling
//	AllocatedBuffer modelInformationBuffer;
//
//	// actual buffer on the GPU
//	AllocatedBuffer indirectDrawBuffer;
//	AllocatedBuffer indirectDrawBufferAddress;
//
//
//	void initialize_buffers(VkCommandBuffer immediateCommandBuffer, VkFence& immediateFence, VkQueue grapgicsQueue, VkDevice device, VmaAllocator allocator);
//
//	AllocatedBuffer create_buffer(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
//	AllocatedBuffer create_staging_buffer(VmaAllocator allocator, VkDeviceSize allocSize);
//
//	VkDeviceAddress get_address(VkDevice device, VkBuffer buffer);
//};



class VulkanEngine {
public:


	PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT;
	PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT;

	bool _isInitialized{ false };
	int _frameNumber {0};
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

	// Global Lifetime Deletion Queue
	DeletionQueue _mainDeletionQueue;

	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _drawImageBeforeMSAA;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;
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

	// Background Pipeline (Compute)
	VkPipelineLayout _backgroundEffectPipelineLayout;
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };
	VkDescriptorSetLayout computeImageDescriptorSetLayout;
	DescriptorBufferSampler computeImageDescriptorBuffer;

	// Generic Fullscreen Pipeline
	VkPipelineLayout _fullscreenPipelineLayout;
	VkDescriptorSetLayout _fullscreenDescriptorSetLayout;
	DescriptorBufferSampler _fullscreenDescriptorBuffer;
	ShaderObject _fullscreenPipeline;


	void init();
	void cleanup();
	void draw_background(VkCommandBuffer cmd);
	void draw_fullscreen(VkCommandBuffer cmd, AllocatedImage sourceImage,  AllocatedImage targetImage);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void draw();
	void run();

	void resize_swapchain();

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	AllocatedBuffer create_staging_buffer(size_t allocSize);
	void copy_buffer(AllocatedBuffer src, AllocatedBuffer dst, VkDeviceSize size);
	VkDeviceAddress get_buffer_address(AllocatedBuffer buffer);
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	void destroy_buffer(const AllocatedBuffer& buffer);


	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
	
	// Textures
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, size_t dataSize, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	int get_channel_count(VkFormat format);
	void destroy_image(const AllocatedImage& img);

	// Material Pipeline
	MaterialInstance defaultOpaqueMaterial;
	GLTFMetallic_Roughness metallicRoughnessPipelines;
	DrawContext mainDrawContext;

	GLTFMetallic_RoughnessMultiDraw testMultiDraw;


	GPUSceneData sceneData;
	VkDescriptorSetLayout gpuSceneDataDescriptorBufferSetLayout;
	DescriptorBufferUniform gpuSceneDataDescriptorBuffer;
	AllocatedBuffer gpuSceneDataBuffer;
	float globalModelScale{ 1.0f };
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTFMultiDraw>> loadedMultiDrawScenes;
	void update_scene();

private:
	void init_vulkan();
	void init_swapchain();
	void init_draw_images();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_dearimgui();

	void init_pipelines();
	void init_compute_pipelines();
	void init_fullscreen_pipeline();
	void init_default_data();

	void create_swapchain(uint32_t width, uint32_t height);
	void create_draw_images(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void destroy_draw_iamges();
};