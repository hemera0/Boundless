#pragma once
#include "Scene.hpp"
#include "Pipelines.hpp"
#include "BaseRenderPass.hpp"

namespace Boundless {
	struct alignas( 16 ) GBufferPushConstants {
		vk::DeviceAddress m_FrameConstantsBuffer;
		vk::DeviceAddress m_MaterialsBuffer;
		vk::DeviceAddress m_VertexBuffer;
		uint8_t           m_Pad0[8];
		glm::mat4		  m_WorldTransform;
		uint32_t		  m_MaterialIndex;
	};

	struct GBufferDebugPushConstants {
		uint32_t m_GBufferTexture;
		uint32_t m_DepthTexture;
		uint32_t m_GBufferChannel;
	};

	struct LightingPushConstants {
		vk::DeviceAddress m_FrameConstantsBuffer;
		uint32_t		  m_GBufferTexture;
		uint32_t		  m_GBufferDepthTexture;
	};

	struct CompositePushConstants {
		uint32_t m_Texture;
		uint32_t m_ApplyGammaCurve;
		uint32_t m_ApplyTonemapping;
	};

	struct BlitPushConstants {
		uint32_t m_Texture;
	};

	struct SkinningPushConstants {
		vk::DeviceAddress m_VertexBuffer;
		vk::DeviceAddress m_SkinnedVertexBuffer;
		vk::DeviceAddress m_BoneIndicesBuffer;
		vk::DeviceAddress m_BoneWeightsBuffer;
		vk::DeviceAddress m_BoneTransformsBuffer;
		uint32_t		  m_VertexCount;
	};

	struct GBufferOutput {
		ImageHandle m_GBuffer;
		ImageHandle m_DepthBuffer;
	};

	class GBufferPass : public BaseRenderPass {
	public:
		GBufferPass( const Viewport& viewport );
		
		virtual void CreatePassResources( Device& device ) override;
		[[nodiscard]] GBufferOutput Render( CommandBuffer& commandBuffer, Device& device, BufferHandle frameConstantsBuffer, Scene& scene );
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
		void Render( CommandBuffer& commandBuffer, Device& device, BufferHandle frameConstantsBuffer, Scene& scene, const GBufferOutput& gbufferOutput );
	};

	class CompositePass : public BaseRenderPass {
	public:
		CompositePass( const Viewport& viewport );

		virtual void CreatePassResources( Device& device ) override;
		void Render( CommandBuffer& commandBuffer, Device& device, ImageHandle texture );
	};

	class RaytracedReflectionsPass : public BaseRenderPass {
	public:
		RaytracedReflectionsPass( const Viewport& viewport );

		virtual void CreatePassResources( Device& device ) override;
		void Dispatch( CommandBuffer& commandBuffer, Device& device );
	};

	// TODO: maybe switch the scene rendering to use DrawIndirect? 
	// Alternative build a buffer with a 1:3 bitfield [draw, entityIndex]
	class FrustumCullPass : public BaseRenderPass {
	public:

	};

	class SkinningPass : public BaseRenderPass {
	public:
		SkinningPass(  );

		virtual void CreatePassResources( Device& device ) override;
		void Dispatch( CommandBuffer& commandBuffer, Device& device, Scene& scene );
	};

	class BloomPass : public BaseRenderPass {
	public:
		BloomPass( const Viewport& viewport );

		virtual void CreatePassResources( Device& device ) override;
		void Dispatch( CommandBuffer& commandBuffer, Device& device, ImageHandle lightingOutput );
	};
}