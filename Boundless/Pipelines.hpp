#pragma once
#include "VkUtil.hpp"
#include "Shaders.hpp"

namespace Boundless {
	class PipelineLayoutBuilder {
	public:
		PipelineLayoutBuilder& SetDescriptorSets(const std::vector<VkDescriptorSetLayout>& descriptorSets) {
			m_DescriptorSets = descriptorSets;
			return *this;
		}

		PipelineLayoutBuilder& SetPushConstants(const std::vector<VkPushConstantRange>& pushConstants) {
			m_PushConstants = pushConstants;
			return *this;
		}

		VkPipelineLayout Build(const VkDevice& device);
	private:
		std::vector<VkDescriptorSetLayout> m_DescriptorSets{};
		std::vector<VkPushConstantRange> m_PushConstants{};
	};

	class PipelineBuilder {
	public:
		PipelineBuilder();

		PipelineBuilder& SetShaderBlobs(const std::vector<std::pair<IDxcBlob*, VkShaderStageFlagBits>>& shaders) {
			m_ShaderBlobs = shaders;
			return *this;
		}

		PipelineBuilder& SetColorAttachmentFormats(const std::vector<VkFormat>& colorFormats) {
			m_ColorFormats = colorFormats;
			return *this;
		}

		PipelineBuilder& SetDepthAttachmentFormat(VkFormat depthFormat) {
			m_DepthFormat = depthFormat;
			return *this;
		}

		PipelineBuilder& SetPipelineLayout(const VkPipelineLayout& pipelineLayout) {
			m_PipelineLayout = pipelineLayout;
			return *this;
		}

		PipelineBuilder& SetInputAttributeDescriptions(const std::vector<VkVertexInputAttributeDescription>& inputAttributeDescriptions) {
			m_InputAttributeDescriptions = inputAttributeDescriptions;
			return *this;
		}

		PipelineBuilder& SetInputBindingDescriptions(const std::vector<VkVertexInputBindingDescription>& inputBindingDescriptions) {
			m_InputBindingDescriptions = inputBindingDescriptions;
			return *this;
		}

		PipelineBuilder& SetMultisampleState(const VkPipelineMultisampleStateCreateInfo& multisampleState) {
			m_MultisampleState = multisampleState;
			return *this;
		}
		PipelineBuilder& SetInputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& inputAssemblyState) {
			m_InputAssemblyState = inputAssemblyState;
			return *this;
		}

		PipelineBuilder& SetRasterizationState(const VkPipelineRasterizationStateCreateInfo& rasterizationState) {
			m_RasterizationState = rasterizationState;
			return *this;
		}

		PipelineBuilder& SetDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& depthStencilState) {
			m_DepthStencilState = depthStencilState;
			return *this;
		}

		PipelineBuilder& SetDynamicState(const VkPipelineDynamicStateCreateInfo& dynamicState) {
			m_DynamicState = dynamicState;
			return *this;
		}

		PipelineBuilder& SetColorBlendAttachmentStates(const std::vector<VkPipelineColorBlendAttachmentState>& colorBlendAttachmentStates) {
			m_ColorBlendAttachmentStates = colorBlendAttachmentStates;
			return *this;
		}

		PipelineBuilder& SetColorBlendState(const VkPipelineColorBlendStateCreateInfo& colorBlendState) {
			m_ColorBlendState = colorBlendState;
			return *this;
		}

		VkPipeline Build(const VkDevice& device, const VkSwapchainKHR& swapchain);
	private:
		std::vector<std::pair<IDxcBlob*, VkShaderStageFlagBits>> m_ShaderBlobs{};

		std::vector<VkFormat> m_ColorFormats{};
		VkFormat m_DepthFormat{};

		VkPipelineLayout m_PipelineLayout{};
		std::vector<VkVertexInputAttributeDescription> m_InputAttributeDescriptions{};
		std::vector<VkVertexInputBindingDescription> m_InputBindingDescriptions{};

		VkPipelineMultisampleStateCreateInfo m_MultisampleState{};
		VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState{};
		VkPipelineRasterizationStateCreateInfo m_RasterizationState{};
		VkPipelineDepthStencilStateCreateInfo m_DepthStencilState{};
		VkPipelineDynamicStateCreateInfo m_DynamicState{};

		std::vector<VkPipelineColorBlendAttachmentState> m_ColorBlendAttachmentStates{};
		VkPipelineColorBlendStateCreateInfo m_ColorBlendState{};
	};
}