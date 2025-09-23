#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	class RenderGraphAttachment {
	public:

	};
	
	class RenderGraphPass {
	public:

		std::vector< RenderGraphAttachment > m_Inputs{};
		std::vector< RenderGraphAttachment > m_Outputs{};
	};

	class RenderGraph {
	public:


	};
}