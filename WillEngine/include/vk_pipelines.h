#pragma once 
#include <vk_types.h>

namespace vkutil {
    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};


class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;

    VkPipelineLayout _pipelineLayout;
    VkFormat _colorAttachmentFormat;

    PipelineBuilder() { clear(); }

    void clear();

    VkPipeline build_pipeline(VkDevice device);
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setup_input_assembly(VkPrimitiveTopology topology);
    void setup_rasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
    void setup_multisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples
                            , float minSampleShading, const VkSampleMask* pSampleMask
							, VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable);
    void setup_blending(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable);
    void setup_renderer(VkFormat colorattachmentFormat, VkFormat depthAttachmentFormat);
    void setup_depth_stencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp
                            , VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back
                            , float minDepthBounds, float maxDepthBounds);

    void enable_depthtest(bool depthWriteEnable, VkCompareOp op);

    void disable_multisampling();
    void disable_blending();
    void disable_depthtest();

    VkPipelineDynamicStateCreateInfo generate_dynamic_states(VkDynamicState states[], uint32_t count);


};
