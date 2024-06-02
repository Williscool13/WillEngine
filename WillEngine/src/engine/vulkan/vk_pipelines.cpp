#include <vk_pipelines.h	>



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



void vkutil::load_shader(std::string path, char* &data, size_t &size ) {
	VkShaderCreateInfoEXT shaderCreateInfo{};
	// open the file. With cursor at the end
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (file.is_open()) {
		size = file.tellg();
		// put file cursor at beginning
		file.seekg(0);
		// load the entire file into the buffer
		data = new char[size];
		file.read(data, size);
		file.close();
		assert(size > 0);
	}
}

void vkutil::create_shader_objects(
	std::string vertexShader
	, std::string fragmentShader
	, VkDevice device
	, VkShaderEXT* shaders
	, uint32_t descriptorSetCount
	, VkDescriptorSetLayout* descriptorLayout
	, uint32_t pushConstantRangeCount
	, VkPushConstantRange* pushConstantRanges
) 
{
	VkShaderCreateInfoEXT shaderCreateInfos[2]{};
	size_t shaderCodeSizes[2]{};
	char* shaderCodes[2]{};

	vkutil::load_shader(vertexShader, shaderCodes[0], shaderCodeSizes[0]);
	vkutil::load_shader(fragmentShader, shaderCodes[1], shaderCodeSizes[1]);

	shaderCreateInfos[0].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
	shaderCreateInfos[0].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
	shaderCreateInfos[0].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
	shaderCreateInfos[0].pCode = shaderCodes[0];
	shaderCreateInfos[0].codeSize = shaderCodeSizes[0];
	shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderCreateInfos[0].nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderCreateInfos[0].pName = "main";
	shaderCreateInfos[0].setLayoutCount = descriptorSetCount;
	shaderCreateInfos[0].pSetLayouts = descriptorLayout;
	shaderCreateInfos[0].pushConstantRangeCount = pushConstantRangeCount;
	shaderCreateInfos[0].pPushConstantRanges = pushConstantRanges;

	shaderCreateInfos[1].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
	shaderCreateInfos[1].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
	shaderCreateInfos[1].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
	shaderCreateInfos[1].pCode = shaderCodes[1];
	shaderCreateInfos[1].codeSize = shaderCodeSizes[1];
	shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderCreateInfos[1].nextStage = 0;
	shaderCreateInfos[1].pName = "main";
	shaderCreateInfos[1].setLayoutCount = descriptorSetCount;
	shaderCreateInfos[1].pSetLayouts = descriptorLayout;
	shaderCreateInfos[1].pushConstantRangeCount = pushConstantRangeCount;
	shaderCreateInfos[1].pPushConstantRanges = pushConstantRanges;

	// TODO: OPTIMIZE THIS
	PFN_vkCreateShadersEXT vkCreateShadersEXT = reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(device, "vkCreateShadersEXT"));
	VK_CHECK(vkCreateShadersEXT(device, 2, shaderCreateInfos, nullptr, shaders));

	delete[] shaderCodes[0];
	delete[] shaderCodes[1];
}
bool vkutil::load_shader_module(VkDevice device, VkShaderEXT* shaders, VkDescriptorSetLayout* descriptorLayout) {
	std::vector<const char*> shaderPaths = {
		"shaders/mesh.vert.spv",
		"shaders/mesh.frag.spv"
	};

	VkShaderCreateInfoEXT shaderCreateInfos[2]{};
	size_t shaderCodeSizes[2]{};
	char* shaderCodes[2]{};

	for (int i = 0; i < 2; i++) {

		// open the file. With cursor at the end
		std::ifstream file(shaderPaths[i], std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			return false;
		}

		shaderCodeSizes[i] = file.tellg();
		// put file cursor at beginning
		file.seekg(0);
		// load the entire file into the buffer
		shaderCodes[i] = new char[shaderCodeSizes[i]];
		file.read(shaderCodes[i], shaderCodeSizes[i]);
		// now that the file is loaded into the buffer, we can close it
		file.close();

		//shaderCodes[i] = (char*)buffer.data();
		//shaderCodeSizes[i] = buffer.size() * sizeof(uint32_t);

		shaderCreateInfos[i].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
		shaderCreateInfos[i].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos[i].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
		shaderCreateInfos[i].pCode = shaderCodes[i];
		shaderCreateInfos[i].codeSize = shaderCodeSizes[i];
		shaderCreateInfos[i].pName = "main";
		shaderCreateInfos[i].setLayoutCount = 3;
		shaderCreateInfos[i].pSetLayouts = descriptorLayout;
	}


	shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderCreateInfos[1].nextStage = 0;


	PFN_vkCreateShadersEXT vkCreateShadersEXT = reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(device, "vkCreateShadersEXT"));
	VK_CHECK(vkCreateShadersEXT(device, 2, shaderCreateInfos, nullptr, shaders));

	delete[] shaderCodes[0];
	delete[] shaderCodes[1];
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

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkPipelineCreateFlagBits flags)
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

	pipelineInfo.flags = flags;

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
		return VK_NULL_HANDLE;
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

void PipelineBuilder::setup_blending(PipelineBuilder::BlendMode mode) {
	switch (mode) {
	case BlendMode::ALPHA_BLEND: {
		_colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		_colorBlendAttachment.blendEnable = VK_TRUE;
		_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		break;
	}
	case BlendMode::ADDITIVE_BLEND: {
		_colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		_colorBlendAttachment.blendEnable = VK_TRUE;
		_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		break;
	}
	case BlendMode::NO_BLEND:
		_colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		_colorBlendAttachment.blendEnable = VK_FALSE;
		break;
	}
}



void PipelineBuilder::disable_multisampling()
{
	setup_multisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT, 1.0f, nullptr, VK_FALSE, VK_FALSE);
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

void ShaderObjectPipeline::prepare(VkDevice device)
{
	vkCreateShadersEXT = reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(device, "vkCreateShadersEXT"));
	vkDestroyShaderEXT = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(device, "vkDestroyShaderEXT"));
	vkCmdBindShadersEXT = reinterpret_cast<PFN_vkCmdBindShadersEXT>(vkGetDeviceProcAddr(device, "vkCmdBindShadersEXT"));
	vkGetShaderBinaryDataEXT = reinterpret_cast<PFN_vkGetShaderBinaryDataEXT>(vkGetDeviceProcAddr(device, "vkGetShaderBinaryDataEXT"));

	vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR"));
	vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR"));

	vkCmdSetAlphaToCoverageEnableEXT = reinterpret_cast<PFN_vkCmdSetAlphaToCoverageEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetAlphaToCoverageEnableEXT"));
	vkCmdSetColorBlendEnableEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEnableEXT"));
	vkCmdSetColorWriteMaskEXT = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorWriteMaskEXT"));
	vkCmdSetCullModeEXT = reinterpret_cast<PFN_vkCmdSetCullModeEXT>(vkGetDeviceProcAddr(device, "vkCmdSetCullModeEXT"));
	vkCmdSetDepthBiasEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthBiasEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetDepthBiasEnableEXT"));
	vkCmdSetDepthCompareOpEXT = reinterpret_cast<PFN_vkCmdSetDepthCompareOpEXT>(vkGetDeviceProcAddr(device, "vkCmdSetDepthCompareOpEXT"));
	vkCmdSetDepthTestEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthTestEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnableEXT"));
	vkCmdSetDepthWriteEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthWriteEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetDepthWriteEnableEXT"));
	vkCmdSetFrontFaceEXT = reinterpret_cast<PFN_vkCmdSetFrontFaceEXT>(vkGetDeviceProcAddr(device, "vkCmdSetFrontFaceEXT"));
	vkCmdSetPolygonModeEXT = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(vkGetDeviceProcAddr(device, "vkCmdSetPolygonModeEXT"));
	vkCmdSetPrimitiveRestartEnableEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveRestartEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveRestartEnableEXT"));
	vkCmdSetPrimitiveTopologyEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveTopologyEXT"));
	vkCmdSetRasterizationSamplesEXT = reinterpret_cast<PFN_vkCmdSetRasterizationSamplesEXT>(vkGetDeviceProcAddr(device, "vkCmdSetRasterizationSamplesEXT"));
	vkCmdSetRasterizerDiscardEnableEXT = reinterpret_cast<PFN_vkCmdSetRasterizerDiscardEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetRasterizerDiscardEnableEXT"));
	vkCmdSetSampleMaskEXT = reinterpret_cast<PFN_vkCmdSetSampleMaskEXT>(vkGetDeviceProcAddr(device, "vkCmdSetSampleMaskEXT"));
	vkCmdSetScissorWithCountEXT = reinterpret_cast<PFN_vkCmdSetScissorWithCountEXT>(vkGetDeviceProcAddr(device, "vkCmdSetScissorWithCountEXT"));
	vkCmdSetStencilTestEnableEXT = reinterpret_cast<PFN_vkCmdSetStencilTestEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetStencilTestEnableEXT"));
	vkCmdSetVertexInputEXT = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(vkGetDeviceProcAddr(device, "vkCmdSetVertexInputEXT"));
	vkCmdSetViewportWithCountEXT = reinterpret_cast<PFN_vkCmdSetViewportWithCountEXT>(vkGetDeviceProcAddr(device, "vkCmdSetViewportWithCountEXT"));;

	vkCmdSetAlphaToOneEnableEXT = reinterpret_cast<PFN_vkCmdSetAlphaToOneEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetAlphaToOneEnableEXT"));
	vkCmdSetColorBlendEquationEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEquationEXT"));


}

void ShaderObjectPipeline::bind_viewport(VkCommandBuffer cmd, float width, float height, float minDepth, float maxDepth)
{
VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	//vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetViewportWithCount(cmd, 1, &viewport);
}

void ShaderObjectPipeline::bind_scissor(VkCommandBuffer cmd, int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height)
{
	VkRect2D scissor = {};
	scissor.offset = { offsetX, offsetY };
	scissor.extent = { width, height };
	//vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdSetScissorWithCount(cmd, 1, &scissor);
}

void ShaderObjectPipeline::bind_rasterizaer_discard(VkCommandBuffer cmd, VkBool32 rasterizerDiscardEnable)
{
	vkCmdSetRasterizerDiscardEnable(cmd, rasterizerDiscardEnable);
}

void ShaderObjectPipeline::setup_input_assembly(VkPrimitiveTopology topology)
{
	_topology = topology;
}

void ShaderObjectPipeline::setup_rasterization(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	_polygonMode = polygonMode;
	_cullMode = cullMode;
	_frontFace = frontFace;
}

void ShaderObjectPipeline::setup_multisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples
	, float minSampleShading, const VkSampleMask* pSampleMask, VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable)
{
	throw std::logic_error("The method or operation is not implemented.");
}

void ShaderObjectPipeline::setup_depth(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp
	, VkBool32 depthBiasEnable, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor
	, VkBool32 depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds)
{
	_depthTestEnable = depthTestEnable;
	_depthWriteEnable = depthWriteEnable;
	_compareOp = compareOp;

	_depthBiasEnable = depthBiasEnable;
	_depthBiasConstantFactor = depthBiasConstantFactor;
	_depthBiasClamp = depthBiasClamp;
	_depthBiasSlopeFactor = depthBiasSlopeFactor;

	_depthBoundsTestEnable = depthBoundsTestEnable;
	_minDepthBounds = minDepthBounds;
	_maxDepthBounds = maxDepthBounds;
}

void ShaderObjectPipeline::setup_blending(ShaderObjectPipeline::BlendMode mode)
{
	switch (mode) {
	case BlendMode::ALPHA_BLEND: {
		_colorBlendingEnabled = VK_TRUE;
		_colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		_colorBlendingEquation.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		_colorBlendingEquation.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		_colorBlendingEquation.colorBlendOp = VK_BLEND_OP_ADD;
		_colorBlendingEquation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		_colorBlendingEquation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		_colorBlendingEquation.alphaBlendOp = VK_BLEND_OP_ADD;
		break;
	}
	case BlendMode::ADDITIVE_BLEND: {
		_colorBlendingEnabled = VK_TRUE;
		_colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		_colorBlendingEquation.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		_colorBlendingEquation.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		_colorBlendingEquation.colorBlendOp = VK_BLEND_OP_ADD;
		_colorBlendingEquation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		_colorBlendingEquation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		_colorBlendingEquation.alphaBlendOp = VK_BLEND_OP_ADD;
		break;
	}
	case BlendMode::NO_BLEND:
		_colorBlendingEnabled = VK_FALSE;
		_colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		break;
	}
}

void ShaderObjectPipeline::setup_stencil(VkBool32 stencilTestEnable, VkStencilOpState front, VkStencilOpState back)
{
	throw std::logic_error("The method or operation is not implemented.");
}


void ShaderObjectPipeline::disable_multisampling()
{
	_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	_pSampleMask = 0xFF;
	_alphaToCoverageEnable = VK_FALSE;
	_alphaToOneEnable = VK_FALSE;
}

void ShaderObjectPipeline::enable_depthtesting(bool depthWriteEnable, VkCompareOp op)
{
	setup_depth(
		VK_TRUE, depthWriteEnable, op, // test/write
		VK_FALSE, 0.0f, 0.0f, 0.0f, // bias
		VK_FALSE, 0.0f, 1.0f // bounds
	);
}

void ShaderObjectPipeline::bind_input_assembly(VkCommandBuffer cmd)
{

	vkCmdSetPrimitiveTopologyEXT(cmd, _topology);
	vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);
	// unused, using buffer device address instead
	vkCmdSetVertexInputEXT(cmd, 0, nullptr, 0, nullptr);
}

void ShaderObjectPipeline::bind_rasterization(VkCommandBuffer cmd)
{
	// Draw Mode
	vkCmdSetPolygonModeEXT(cmd, _polygonMode);
	// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#vkCmdSetLineWidth
	vkCmdSetLineWidth(cmd, 1.0f);

	// Culling
	vkCmdSetCullMode(cmd, _cullMode);
	vkCmdSetFrontFace(cmd, _frontFace);
}

void ShaderObjectPipeline::bind_depth_test(VkCommandBuffer cmd) 
{
	vkCmdSetDepthTestEnable(cmd, _depthTestEnable);
	vkCmdSetDepthWriteEnable(cmd, _depthWriteEnable);
	vkCmdSetDepthCompareOp(cmd, _compareOp);
	
	vkCmdSetDepthBoundsTestEnable(cmd, _depthBoundsTestEnable);
	vkCmdSetDepthBounds(cmd, _minDepthBounds, _maxDepthBounds);

	vkCmdSetDepthBiasEnable(cmd, _depthBiasEnable);
	if (_depthBiasEnable) { vkCmdSetDepthBias(cmd, _depthBiasConstantFactor, _depthBiasClamp, _depthBiasSlopeFactor); }
	// only if depthClamp feature is enabled
	//vkCmdSetDepthClampEnableEXT();
}

void ShaderObjectPipeline::bind_stencil(VkCommandBuffer cmd) {
	vkCmdSetStencilTestEnable(cmd, VK_FALSE);
	// below are the 4 functions to set stencil state
	//vkCmdSetStencilOp(); 
	//vkCmdSetStencilCompareMask();
	//vkCmdSetStencilWriteMask();
	//vkCmdSetStencilReference(); 
}

void ShaderObjectPipeline::bind_multisampling(VkCommandBuffer cmd)
{
	//vkCmdSetSampleShadingEnableEXT(cmd, _sampleShadingEnable);
	vkCmdSetRasterizationSamplesEXT(cmd, _rasterizationSamples);
	vkCmdSetSampleMaskEXT(cmd, _rasterizationSamples, &_pSampleMask);
	vkCmdSetAlphaToCoverageEnableEXT(cmd, _alphaToCoverageEnable);
	vkCmdSetAlphaToOneEnableEXT(cmd, _alphaToOneEnable);
}

void ShaderObjectPipeline::bind_blending(VkCommandBuffer cmd)
{
	vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &_colorBlendingEnabled);
	vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &_colorWriteMask);
	vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &_colorBlendingEquation);

	//vkCmdSetBlendConstants for blend equations that use constants
}

void ShaderObjectPipeline::bind_shaders(VkCommandBuffer cmd, uint32_t stageCount, VkShaderStageFlagBits* stages, VkShaderEXT* shaders)
{
	vkCmdBindShadersEXT(cmd, 2, stages, shaders);
}
