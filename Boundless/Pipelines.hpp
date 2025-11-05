#pragma once
#include "VkUtil.hpp"
#include "Shaders.hpp"

namespace Boundless {
	class Device;

	class PipelineLayoutBuilder {
	public:
		PipelineLayoutBuilder& SetDescriptorSets( const std::vector<vk::DescriptorSetLayout>& descriptorSets ) { m_DescriptorSets = descriptorSets; return *this; }
		PipelineLayoutBuilder& SetPushConstants( const std::vector<vk::PushConstantRange>& pushConstants ) { m_PushConstants = pushConstants; return *this; }

		vk::PipelineLayout Build( Device& device );
	private:
		std::vector<vk::DescriptorSetLayout> m_DescriptorSets;
		std::vector<vk::PushConstantRange>   m_PushConstants;
	};

	class ComputePipelineBuilder {
		using ShaderBlobInfo = std::pair<IDxcBlob*, vk::ShaderStageFlagBits>;
	public:
		ComputePipelineBuilder& SetShaderBlob( const ShaderBlobInfo& shader ) { m_ShaderBlob = shader; return *this; }
		ComputePipelineBuilder& SetPipelineLayout( const vk::PipelineLayout& pipelineLayout ) { m_PipelineLayout = pipelineLayout; return *this; }

		vk::Pipeline Build( Device& device );
	private:
		ShaderBlobInfo	   m_ShaderBlob;
		vk::PipelineLayout m_PipelineLayout = {};
	};

	class PipelineBuilder {
		using ShaderBlobInfo = std::pair<IDxcBlob*, vk::ShaderStageFlagBits>;
	public:
		PipelineBuilder();

		PipelineBuilder& SetShaderBlobs( const std::vector<ShaderBlobInfo>& shaders ) { m_ShaderBlobs = shaders; return *this; }
		PipelineBuilder& SetColorAttachmentFormats( const VectorWrapper<vk::Format>& colorFormats ) { m_ColorFormats = colorFormats; return *this; }
		PipelineBuilder& SetDepthAttachmentFormat( vk::Format depthFormat ) { m_DepthFormat = depthFormat; return *this; }
		PipelineBuilder& SetPipelineLayout( const vk::PipelineLayout& pipelineLayout ) { m_PipelineLayout = pipelineLayout; return *this; }
		PipelineBuilder& SetInputAttributeDescriptions( const std::vector<vk::VertexInputAttributeDescription>& inputAttributeDescriptions ) { m_InputAttributeDescriptions = inputAttributeDescriptions; return *this; }
		PipelineBuilder& SetInputBindingDescriptions( const std::vector<vk::VertexInputBindingDescription>& inputBindingDescriptions ) { m_InputBindingDescriptions = inputBindingDescriptions; return *this; }
		PipelineBuilder& SetMultisampleState( const vk::PipelineMultisampleStateCreateInfo& state ) { m_MultisampleState = state; return *this; }
		PipelineBuilder& SetInputAssemblyState( const vk::PipelineInputAssemblyStateCreateInfo& state ) { m_InputAssemblyState = state; return *this; }
		PipelineBuilder& SetRasterizationState( const vk::PipelineRasterizationStateCreateInfo& state ) { m_RasterizationState = state; return *this; }
		PipelineBuilder& SetDepthStencilState( const vk::PipelineDepthStencilStateCreateInfo& state ) { m_DepthStencilState = state; return *this; }
		PipelineBuilder& SetDynamicState( const vk::PipelineDynamicStateCreateInfo& state ) { m_DynamicState = state; return *this; }
		PipelineBuilder& SetColorBlendAttachmentStates( const std::vector<vk::PipelineColorBlendAttachmentState>& states ) { m_ColorBlendAttachmentStates = states; return *this; }
		PipelineBuilder& SetColorBlendState( const vk::PipelineColorBlendStateCreateInfo& state ) { m_ColorBlendState = state; return *this; }
		
		vk::Pipeline Build( Device& device );
	private:
		std::vector<ShaderBlobInfo>						   m_ShaderBlobs;
		std::vector<vk::Format>							   m_ColorFormats;
		vk::Format										   m_DepthFormat = vk::Format::eUndefined;
		std::vector<vk::VertexInputAttributeDescription>   m_InputAttributeDescriptions;
		std::vector<vk::VertexInputBindingDescription>     m_InputBindingDescriptions;
		vk::PipelineMultisampleStateCreateInfo			   m_MultisampleState = {};
		vk::PipelineInputAssemblyStateCreateInfo		   m_InputAssemblyState = {};
		vk::PipelineRasterizationStateCreateInfo		   m_RasterizationState = {};
		vk::PipelineDepthStencilStateCreateInfo			   m_DepthStencilState = {};
		vk::PipelineDynamicStateCreateInfo				   m_DynamicState = {};
		std::vector<vk::PipelineColorBlendAttachmentState> m_ColorBlendAttachmentStates;
		vk::PipelineColorBlendStateCreateInfo			   m_ColorBlendState = {};
		vk::PipelineLayout								   m_PipelineLayout = {};
	};
}