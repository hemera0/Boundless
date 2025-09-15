#include "Pipelines.hpp"

VkPipelineLayout Boundless::PipelineLayoutBuilder::Build( const VkDevice& device ) {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = 0; // Optional
	pipelineLayoutCreateInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	if ( !m_DescriptorSets.empty() ) {
		pipelineLayoutCreateInfo.setLayoutCount = static_cast< uint32_t >( m_DescriptorSets.size() );
		pipelineLayoutCreateInfo.pSetLayouts = m_DescriptorSets.data();
	}

	if ( !m_PushConstants.empty() ) {
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast< uint32_t >( m_PushConstants.size() );
		pipelineLayoutCreateInfo.pPushConstantRanges = m_PushConstants.data();
	}

	VkPipelineLayout ret = VK_NULL_HANDLE;
	VkResult result = vkCreatePipelineLayout( device, &pipelineLayoutCreateInfo, nullptr, &ret );
	if ( result != VK_SUCCESS ) {
		printf( "Failed to create pipleline layout: %d\n", result );
		return VK_NULL_HANDLE;
	}

	return ret;
}

Boundless::PipelineBuilder::PipelineBuilder() {
	m_MultisampleState = VkUtil::PipelineDefaultMultiSampleState();
	m_InputAssemblyState = VkUtil::PipelineDefaultInputAssemblyState();
	m_RasterizationState = VkUtil::PipelineDefaultRasterizationState();
	m_DepthStencilState = VkUtil::PipelineDefaultDepthStencilState();
	m_DynamicState = VkUtil::PipelineDefaultDynamicState();
	m_ColorBlendAttachmentStates = { VkUtil::PipelineDefaultColorBlendAttachmentState() };
}

VkPipeline Boundless::PipelineBuilder::Build( const VkDevice& device, const VkSwapchainKHR& swapchain ) {
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
	std::vector<VkShaderModuleCreateInfo> shaderModules{};

	shaderStages.resize( m_ShaderBlobs.size() );
	shaderModules.resize( m_ShaderBlobs.size() );

	// Adapted from zeux - https://github.com/zeux/niagara/blob/master/src/shaders.cpp#L531
	for ( auto i = 0; i < m_ShaderBlobs.size(); i++ ) {
		const auto& [blob, shaderStage] = m_ShaderBlobs[ i ];

		auto& module = shaderModules[ i ];
		module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		module.codeSize = blob->GetBufferSize();
		module.pCode = reinterpret_cast< const uint32_t* >( blob->GetBufferPointer() );

		auto& stage = shaderStages[ i ];
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = shaderStage; // TODO: FIX ME.... shader->GetStageFlags();
		stage.pName = "main";
		stage.pNext = &module;
	}

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	if ( m_InputBindingDescriptions.empty() && m_InputAttributeDescriptions.empty() ) {
		vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
		vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
		vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;
	} else {
		vertexInputCreateInfo.vertexBindingDescriptionCount = static_cast< uint32_t >( m_InputBindingDescriptions.size() );
		vertexInputCreateInfo.pVertexBindingDescriptions = m_InputBindingDescriptions.data();
		vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast< uint32_t >( m_InputAttributeDescriptions.size() );
		vertexInputCreateInfo.pVertexAttributeDescriptions = m_InputAttributeDescriptions.data();
	}

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = 0.f;
	viewport.height = 0.f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRenderingCreateInfo renderingPipelineCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	renderingPipelineCreateInfo.colorAttachmentCount = static_cast< uint32_t >( m_ColorFormats.size() );
	renderingPipelineCreateInfo.pColorAttachmentFormats = m_ColorFormats.empty() ? nullptr : m_ColorFormats.data();
	renderingPipelineCreateInfo.depthAttachmentFormat = m_DepthFormat;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.stageCount = static_cast< uint32_t >( shaderStages.size() );
	pipelineCreateInfo.pStages = shaderStages.data();

	// Todo remove: This is bad.
	m_ColorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	m_ColorBlendState.logicOpEnable = VK_FALSE;
	m_ColorBlendState.logicOp = VK_LOGIC_OP_COPY; // Optional
	m_ColorBlendState.attachmentCount = static_cast< uint32_t >( m_ColorBlendAttachmentStates.size() );
	m_ColorBlendState.pAttachments = m_ColorBlendAttachmentStates.data();

	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &m_InputAssemblyState;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &m_RasterizationState;
	pipelineCreateInfo.pMultisampleState = &m_MultisampleState;
	pipelineCreateInfo.pDepthStencilState = &m_DepthStencilState;
	pipelineCreateInfo.pColorBlendState = &m_ColorBlendState;
	pipelineCreateInfo.pDynamicState = &m_DynamicState;
	pipelineCreateInfo.layout = m_PipelineLayout;
	pipelineCreateInfo.renderPass = VK_NULL_HANDLE; // Replaced by Dynamic Rendering.
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineCreateInfo.basePipelineIndex = -1; // Optional
	pipelineCreateInfo.pNext = &renderingPipelineCreateInfo;

	VkPipeline ret = VK_NULL_HANDLE;
	VkResult result = vkCreateGraphicsPipelines( device, nullptr, 1, &pipelineCreateInfo, nullptr, &ret );

	if ( result != VK_SUCCESS ) {
		printf( "Failed to create pipeline: %d\n", result );
		return VK_NULL_HANDLE;
	}

	return ret;
}