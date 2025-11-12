#include "Pch.hpp"
#include "Pipelines.hpp"
#include "Device.hpp"

namespace Boundless {
	vk::PipelineLayout PipelineLayoutBuilder::Build( Device& device ) {
		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.setSetLayouts( m_DescriptorSets );
		pipelineLayoutCreateInfo.setPushConstantRanges( m_PushConstants );
	
		return device->createPipelineLayout( pipelineLayoutCreateInfo );
	}

	PipelineBuilder::PipelineBuilder() {
		m_MultisampleState			 = vk_util::PipelineDefaultMultiSampleState();
		m_InputAssemblyState		 = vk_util::PipelineDefaultInputAssemblyState();
		m_RasterizationState		 = vk_util::PipelineDefaultRasterizationState();
		m_DepthStencilState			 = vk_util::PipelineDefaultDepthStencilState();
		m_DynamicState				 = vk_util::PipelineDefaultDynamicState();
		m_ColorBlendAttachmentStates = { vk_util::PipelineDefaultColorBlendAttachmentState() };
	}

	vk::Pipeline ComputePipelineBuilder::Build( Device& device ) {
		const auto& [ blob, stage ] = m_ShaderBlob;

		vk::ShaderModuleCreateInfo shaderModule = {};
		shaderModule.codeSize = blob->GetBufferSize();
		shaderModule.pCode = reinterpret_cast< const uint32_t* >( blob->GetBufferPointer() );

		vk::PipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.stage = stage;
		shaderStage.pName = "main";
		shaderStage.pNext = &shaderModule;

		vk::ComputePipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.setStage( shaderStage )
			.setLayout( m_PipelineLayout );
		
		const auto& [ result, value ] = device->createComputePipeline( nullptr, pipelineCreateInfo );
		if ( result != vk::Result::eSuccess ) {
			return VK_NULL_HANDLE;
		}

		return value;
	}

	vk::Pipeline PipelineBuilder::Build( Device& device ) {
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{};
		std::vector<vk::ShaderModuleCreateInfo> shaderModules{};

		shaderStages.resize( m_ShaderBlobs.size() );
		shaderModules.resize( m_ShaderBlobs.size() );

		// Adapted from zeux - https://github.com/zeux/niagara/blob/master/src/shaders.cpp#L531
		for ( auto i = 0; i < m_ShaderBlobs.size(); i++ ) {
			const auto& [blob, shaderStage] = m_ShaderBlobs[ i ];

			auto& module = shaderModules[ i ];
			module.codeSize = blob->GetBufferSize();
			module.pCode = reinterpret_cast< const uint32_t* >( blob->GetBufferPointer() );

			auto& stage = shaderStages[ i ];
			stage.stage = shaderStage; // TODO: FIX ME.... shader->GetStageFlags();
			stage.pName = "main";
			stage.pNext = &module;
		}

		vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.setVertexAttributeDescriptions( m_InputAttributeDescriptions );
		vertexInputCreateInfo.setVertexBindingDescriptions( m_InputBindingDescriptions );

		vk::PipelineViewportStateCreateInfo viewportStateCreateInfo = {};
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.scissorCount = 1;

		vk::PipelineRenderingCreateInfo renderingPipelineCreateInfo = {};
		renderingPipelineCreateInfo.setColorAttachmentFormats( m_ColorFormats );
		renderingPipelineCreateInfo.setDepthAttachmentFormat( m_DepthFormat );

		// Todo remove: This is bad.
		m_ColorBlendState = vk::PipelineColorBlendStateCreateInfo{};
		m_ColorBlendState.setAttachments( m_ColorBlendAttachmentStates );

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.setStages( shaderStages )
			.setPVertexInputState( &vertexInputCreateInfo )
			.setPInputAssemblyState( &m_InputAssemblyState )
			.setPViewportState( &viewportStateCreateInfo )
			.setPRasterizationState( &m_RasterizationState )
			.setPMultisampleState( &m_MultisampleState )
			.setPDepthStencilState( &m_DepthStencilState )
			.setPColorBlendState( &m_ColorBlendState )
			.setPDynamicState( &m_DynamicState )
			.setLayout( m_PipelineLayout )
			.setPNext( &renderingPipelineCreateInfo );

		const auto& [ result, value ] = device->createGraphicsPipeline( nullptr, pipelineCreateInfo );
		if ( result != vk::Result::eSuccess ) {
			return VK_NULL_HANDLE;
		}

		return value;
	}
}