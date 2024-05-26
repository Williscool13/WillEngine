﻿#include <vk_pipelines.h>
#include <fstream>
#include <vk_initializers.h>


bool vkutil::load_shader_module(const char* filePath,
    VkDevice device,
    VkShaderModule* outShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}


void PipelineBuilder::clear()
{
	_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	_colorBlendAttachment = {}; // defines traditional alpha blending
	_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	_pipelineLayout = {};
	_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	_shaderStages.clear();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
    // Viewport, details not necessary here (dynamic rendering)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Color Blending
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &_colorBlendAttachment;

    // Vertex Attribute Input (unused)
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo 
        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };


    // Build Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.pNext = &_renderInfo; // for pipeline creation w/ dynamic rendering

    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pColorBlendState = &colorBlending;

    pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pDepthStencilState = &_depthStencil;
    pipelineInfo.layout = _pipelineLayout;


    // Dynamic state
    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo = generate_dynamic_states(state, 2);
    pipelineInfo.pDynamicState = &dynamicInfo;

    // Create Pipeline
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
        nullptr, &newPipeline)
        != VK_SUCCESS) {
        fmt::print("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    else {
        return newPipeline;
    }

    return VkPipeline();
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    _shaderStages.clear();

    _shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
	);

    _shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
	);
}

void PipelineBuilder::setup_input_assembly(VkPrimitiveTopology topology)
{
    _inputAssembly.topology = topology;
	_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::setup_rasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    // Draw Mode
    _rasterizer.polygonMode = polygonMode;
    _rasterizer.lineWidth = 1.0f;

    // Culling
    _rasterizer.cullMode = cullMode;
    _rasterizer.frontFace = frontFace;
}

void PipelineBuilder::setup_multisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading, const VkSampleMask* pSampleMask, VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable)
{
    _multisampling.sampleShadingEnable = sampleShadingEnable;
	
	_multisampling.rasterizationSamples = rasterizationSamples;
	_multisampling.minSampleShading = minSampleShading;
	_multisampling.pSampleMask = pSampleMask;
    // A2C
	_multisampling.alphaToCoverageEnable = alphaToCoverageEnable;
	_multisampling.alphaToOneEnable = alphaToOneEnable;
}

void PipelineBuilder::setup_blending(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable)
{
    _colorBlendAttachment.colorWriteMask = colorWriteMask;
	_colorBlendAttachment.blendEnable = blendEnable;
}

void PipelineBuilder::setup_renderer(VkFormat colorattachmentFormat, VkFormat depthAttachmentFormat)
{
    // Color Format
    _colorAttachmentFormat = colorattachmentFormat;
    _renderInfo.colorAttachmentCount = 1;
    _renderInfo.pColorAttachmentFormats = &_colorAttachmentFormat;

    // Depth Format
	_renderInfo.depthAttachmentFormat = depthAttachmentFormat;
}

void PipelineBuilder::setup_depth_stencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back, float minDepthBounds, float maxDepthBounds)
{
    _depthStencil.depthTestEnable = depthTestEnable;
	_depthStencil.depthWriteEnable = depthWriteEnable;
	_depthStencil.depthCompareOp = compareOp;
	_depthStencil.depthBoundsTestEnable = depthBoundsTestEnable;
	_depthStencil.stencilTestEnable = stencilTestEnable;
	_depthStencil.front = front;
	_depthStencil.back = back;
	_depthStencil.minDepthBounds = minDepthBounds;
	_depthStencil.maxDepthBounds = maxDepthBounds;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op) {
    setup_depth_stencil(
        VK_TRUE, depthWriteEnable, op,
        VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
    );
}


void PipelineBuilder::disable_multisampling()
{
    setup_multisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT, 1.0f, nullptr, VK_FALSE, VK_FALSE);
}

void PipelineBuilder::disable_blending()
{
    setup_blending(
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        VK_FALSE
    );
}

void PipelineBuilder::disable_depthtest()
{
    setup_depth_stencil(
		VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER,
		VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f
	);
}

VkPipelineDynamicStateCreateInfo PipelineBuilder::generate_dynamic_states(VkDynamicState states[], uint32_t count)
{
    VkPipelineDynamicStateCreateInfo dynamicInfo = { };
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.pDynamicStates = states;
    dynamicInfo.dynamicStateCount = count;
    return dynamicInfo;
}



