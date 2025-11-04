#pragma once
#include "Scene.hpp"
#include "Pipelines.hpp"
#include "BaseRenderPass.hpp"

namespace Boundless {
	struct GBufferPushConstants {
		VkDeviceAddress m_SceneBuffer;
		VkDeviceAddress m_MaterialsBuffer;
		VkDeviceAddress m_VertexBuffer;
		uint32_t		m_MaterialIndex;
	};

	struct GBufferDebugPushConstants {
		uint32_t m_GBufferTexture;
		uint32_t m_DepthTexture;
		uint32_t m_GBufferChannel;
	};

	struct LightingPushConstants {
		VkDeviceAddress m_SceneBuffer;
		uint32_t		m_GBufferTexture;
		uint32_t		m_GBufferDepthTexture;
	};

	struct CompositePushConstants {
		uint32_t m_Texture;
		uint32_t m_ApplyGammaCurve;
		uint32_t m_ApplyTonemapping;
	};

	struct BlitPushConstants {
		uint32_t m_Texture;
	};

	class GBufferPass : public BaseRenderPass {
	public:
		GBufferPass( const Viewport& viewport );
		
		virtual void CreatePassResources( Device& device ) override;
		void Render( CommandBuffer& commandBuffer, Device& device, Scene& scene );
	};

	class GBufferDebugPass : public BaseRenderPass {
	public:
		GBufferDebugPass( const Viewport& viewport );

		virtual void CreatePassResources( Device& device ) override;
		void Render( CommandBuffer& commandBuffer, Device& device, ImageHandle gbufferTexture, ImageHandle depthTexture, uint32_t channelIndex );
	};

	class LightingPass : public BaseRenderPass {
	public:
		LightingPass( const Viewport& viewport );
	
		virtual void CreatePassResources( Device& device ) override;
		void Render( CommandBuffer& commandBuffer, Device& device, Scene& scene, ImageHandle gbufferTexture, ImageHandle depthTexture );
	};

	class CompositePass : public BaseRenderPass {
	public:
		CompositePass( const Viewport& viewport );

		virtual void CreatePassResources( Device& device ) override;
		void Render( CommandBuffer& commandBuffer, Device& device, ImageHandle texture );
	};
}