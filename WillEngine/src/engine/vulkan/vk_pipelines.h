#pragma once 
#include "vk_types.h"
#include "will_engine.h"
#include <vk_initializers.h>
#include <vk_descriptor_buffer.h>

namespace vkutil {
    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
    void load_shader(std::string path, char*& data, size_t& size);
    void create_shader_objects(
        std::string vertexShader, std::string fragmentShader
        , VkDevice device, VkShaderEXT* shaders
        , uint32_t descriptorSetCount, VkDescriptorSetLayout* descriptorLayout
        , uint32_t pushConstantRangeCount, VkPushConstantRange* pushConstantRanges);
};

class ShaderObject {
public:
    enum class BlendMode {
        ALPHA_BLEND,
        ADDITIVE_BLEND,
        NO_BLEND
    };

    uint32_t _shaderCount = 3;
    VkShaderStageFlagBits _stages[3];
    VkShaderEXT _shaders[3];


    void init(VkDevice device);
    void init_input_assembly(VkPrimitiveTopology topology);
    void init_vertex_input(VkVertexInputBindingDescription2EXT vertex_description, std::vector<VkVertexInputAttributeDescription2EXT> attribute_descriptions);
    void init_rasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
    void init_multisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples
							, float minSampleShading, const VkSampleMask* pSampleMask
							, VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable);
    // i cant find any set functions for this. maybe it is not needed
    //void setup_renderer(VkFormat colorattachmentFormat, VkFormat depthAttachmentFormat);
    void init_depth(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp
                            , VkBool32 depthBiasEnable, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor
							, VkBool32 depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds);

    void init_stencil(VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back);
    void init_blending(ShaderObject::BlendMode mode);

    // shortcut setup functions
    void disable_multisampling();
    void enable_msaa(VkSampleCountFlagBits samples);
    void enable_depthtesting(bool depthWriteEnable, VkCompareOp op);
    void disable_depthtesting();


    // replaces dynamic states set up for original pipeline
    void bind_viewport(VkCommandBuffer cmd, float width, float height, float minDepth, float maxDepth);
    void bind_scissor(VkCommandBuffer cmd, int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height);
    // should be disabled usually
    //  https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#vkCmdSetRasterizerDiscardEnable
    void bind_rasterizaer_discard(VkCommandBuffer cmd, VkBool32 rasterizerDiscardEnable);
    void bind_input_assembly(VkCommandBuffer cmd);
    void bind_rasterization(VkCommandBuffer cmd);
    void bind_depth_test(VkCommandBuffer cmd);
    void bind_stencil(VkCommandBuffer cmd);
    void bind_multisampling(VkCommandBuffer cmd);
    void bind_blending(VkCommandBuffer cmd);
    void bind_shaders(VkCommandBuffer cmd);

private:
    // input assembly
    VkPrimitiveTopology _topology;
    bool _vertexInputEnabled = false;
    VkVertexInputBindingDescription2EXT _vertex_description;
    std::vector<VkVertexInputAttributeDescription2EXT> _attribute_descriptions;
    // rasterization
    VkPolygonMode _polygonMode;
    VkCullModeFlags _cullMode;
    VkFrontFace _frontFace;
    // multisampling
    VkSampleCountFlagBits _rasterizationSamples;
    uint32_t _pSampleMask; // not currently using sample mask, but if using need to manage memory
    VkBool32 _alphaToCoverageEnable;
    VkBool32 _alphaToOneEnable;
    // blending
    VkBool32 _colorBlendingEnabled;
    VkColorComponentFlags _colorWriteMask;
    VkColorBlendEquationEXT _colorBlendingEquation{};
    // depth
    VkBool32 _depthTestEnable;
    VkBool32 _depthWriteEnable;
    VkCompareOp _compareOp;
    // --
    VkBool32 _depthBiasEnable;
    float _depthBiasConstantFactor;
    float _depthBiasClamp;
    float _depthBiasSlopeFactor;
    // --
    VkBool32 _depthBoundsTestEnable;
    float _minDepthBounds;
    float _maxDepthBounds;

    // stencil
    //VkBool32 _stencilTestEnable;
    //VkStencilOpState _front;
    //VkStencilOpState _back;

    PFN_vkCreateShadersEXT vkCreateShadersEXT{ VK_NULL_HANDLE };
    PFN_vkDestroyShaderEXT vkDestroyShaderEXT{ VK_NULL_HANDLE };
    PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT{ VK_NULL_HANDLE };
    PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT{ VK_NULL_HANDLE };

    // VK_EXT_shader_objects requires render passes to be dynamic
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR{ VK_NULL_HANDLE };
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR{ VK_NULL_HANDLE };

    // With VK_EXT_shader_object pipeline state must be set at command buffer creation using these functions
    PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT{ VK_NULL_HANDLE };

    PFN_vkCmdSetAlphaToOneEnableEXT vkCmdSetAlphaToOneEnableEXT{ VK_NULL_HANDLE };
    PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT{ VK_NULL_HANDLE };


    PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT{ VK_NULL_HANDLE };
};