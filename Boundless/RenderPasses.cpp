#include "Pch.hpp"
#include "RenderPasses.hpp"
#include "Engine.hpp"

namespace Boundless {
	// ------------
	// GBuffer Pass
	// ------------
	GBufferPass::GBufferPass( const Viewport& viewport ) : BaseRenderPass(viewport, "GBuffer Pass" ) {
		RenderTarget( Image::Desc {
				.m_Width = viewport.Size.width,
				.m_Height = viewport.Size.height,
				.m_Format = vk::Format::eR32G32B32A32Sfloat,
				.m_Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
			} );

		DepthTarget( Image::Desc {
				.m_Width = viewport.Size.width,
				.m_Height = viewport.Size.height,
				.m_Format = vk::Format::eD32Sfloat,
				.m_Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment
			} );
	}

	void GBufferPass::CreatePassResources( Device& device ) {
		BaseRenderPass::CreatePassResources( device );
		
		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.GBufferPixelShader, vk::ShaderStageFlagBits::eFragment }, { g_EngineShaders.GBufferVertexShader , vk::ShaderStageFlagBits::eVertex } } )
			.Build( device );
	}

	GBufferOutput GBufferPass::Render( CommandBuffer& commandBuffer, Device& device, BufferHandle frameConstantsBuffer, Scene& scene ) {
		BeginRendering( commandBuffer, device );
		
		commandBuffer.SetScissorAndViewport(m_Viewport);
		commandBuffer.BindPipeline(m_Pipeline);

		commandBuffer->setCullMode( vk::CullModeFlagBits::eBack );

		auto& registry = scene.GetRegistry();

		for ( entt::entity entity : registry.group<Mesh>() ) {
			const Mesh& mesh = registry.get<Mesh>( entity );
			if(mesh.m_Indices.empty())
				continue;

			// TODO: Make transforms not optional... They should be the first component on every entity.
			glm::mat4 modelMatrix( 1.f );

			// if ( Transform* transform = registry.try_get<Transform>( entity ) ) {
			// 	modelMatrix = transform->GetWorldTransform();
			// }
			
			vk::DeviceAddress sceneUniforms  = device.GetBuffer( frameConstantsBuffer ).GetDeviceAddress();
			vk::DeviceAddress sceneMaterials = device.GetBuffer( scene.GetMaterialBuffer() ).GetDeviceAddress();
			vk::DeviceAddress vertexBuffer   = device.GetBuffer( mesh.m_VertexBuffer ).GetDeviceAddress();
			uint32_t materialIndex		   = mesh.m_Material;

			GBufferPushConstants pc = {
				sceneUniforms,
				sceneMaterials,
				vertexBuffer,
				materialIndex
			};

			commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );

			Buffer& indexBuffer = device.GetBuffer( mesh.m_IndexBuffer );
			commandBuffer.BindIndexBuffer( indexBuffer );
			
			commandBuffer->drawIndexed( uint32_t( mesh.m_Indices.size() ), 1, 0, 0, 0 );
		}

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );

		return GBufferOutput{ m_RenderTargetViews[ 0 ], m_DepthTargetView };
	}

	// ------------------
	// GBuffer Debug Pass
	// ------------------
	GBufferDebugPass::GBufferDebugPass( const Viewport& viewport ) : BaseRenderPass( viewport, "GBuffer Debug Pass" ) { 
		RenderTarget( Image::Desc{
			.m_Width = viewport.Size.width,
			.m_Height = viewport.Size.height,
			.m_Format = vk::Format::eR32G32B32A32Sfloat,
			.m_Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
		} );
	}
	
	void GBufferDebugPass::CreatePassResources( Device& device ) { 
		BaseRenderPass::CreatePassResources(device);

		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.GBufferDebugPixelShader, vk::ShaderStageFlagBits::eFragment }, { g_EngineShaders.FullscreenTriVertexShader, vk::ShaderStageFlagBits::eVertex } } )
			.Build( device );
	}
	
	void GBufferDebugPass::Render( CommandBuffer& commandBuffer, Device& device, ImageHandle gbufferTexture, ImageHandle depthTexture, uint32_t channelIndex ) {
		BeginRendering( commandBuffer, device );

		commandBuffer.SetScissorAndViewport( float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );
		commandBuffer.BindPipeline(m_Pipeline);

		GBufferDebugPushConstants pc = {
			.m_GBufferTexture = uint32_t( gbufferTexture ),
			.m_DepthTexture = uint32_t( depthTexture ),
			.m_GBufferChannel = channelIndex
		};

		commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );
		
		commandBuffer->setCullMode( vk::CullModeFlagBits::eFront );
		commandBuffer->draw( 3, 1, 0, 0 );

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );
	}

	// -------------
	// Lighting Pass
	// -------------
	LightingPass::LightingPass( const Viewport& viewport ) : BaseRenderPass( viewport, "Lighting Pass" ) { 
		RenderTarget( Image::Desc{
			.m_Width = viewport.Size.width,
			.m_Height = viewport.Size.height,
			.m_Format = vk::Format::eR8G8B8A8Unorm,
			.m_Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
		} );
	}

	void LightingPass::CreatePassResources( Device& device ) { 
		BaseRenderPass::CreatePassResources( device );

		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.LightingPixelShader, vk::ShaderStageFlagBits::eFragment }, { g_EngineShaders.FullscreenTriVertexShader, vk::ShaderStageFlagBits::eVertex } } )
			.Build( device );
	}

	void LightingPass::Render( CommandBuffer& commandBuffer, Device& device, BufferHandle frameConstantsBuffer, Scene& scene, const GBufferOutput& gbufferOutput ) {
		BeginRendering( commandBuffer, device );

		commandBuffer.SetScissorAndViewport( float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );
		commandBuffer.BindPipeline( m_Pipeline );

		vk::DeviceAddress frameConstants = device.GetBuffer( frameConstantsBuffer ).GetDeviceAddress();

		LightingPushConstants pc = {
			.m_FrameConstantsBuffer = frameConstants,
			.m_GBufferTexture = uint32_t( gbufferOutput.m_GBuffer ),
			.m_GBufferDepthTexture = uint32_t( gbufferOutput.m_DepthBuffer ),
		};

		commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );

		commandBuffer->setCullMode( vk::CullModeFlagBits::eFront );
		commandBuffer->draw( 3, 1, 0, 0 );

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );
	}

	// -------------
	// Compsite Pass
	// -------------
	CompositePass::CompositePass( const Viewport& viewport ) : BaseRenderPass( viewport, "Composite Pass" ) { 
		RenderTarget( Image::Desc{
			.m_Width = viewport.Size.width,
			.m_Height = viewport.Size.height,
			.m_Format = vk::Format::eR8G8B8A8Unorm,
			.m_Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
		} );
	}

	void CompositePass::CreatePassResources( Device& device ) { 
		BaseRenderPass::CreatePassResources(device);
	
		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.CompositePixelShader, vk::ShaderStageFlagBits::eFragment }, { g_EngineShaders.FullscreenTriVertexShader, vk::ShaderStageFlagBits::eVertex } } )
			.Build( device );
	}
	
	void CompositePass::Render( CommandBuffer& commandBuffer, Device& device, ImageHandle texture ) { 
		BeginRendering( commandBuffer, device );

		commandBuffer.SetScissorAndViewport( float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );
		commandBuffer.BindPipeline( m_Pipeline );

		CompositePushConstants pc = {
			.m_Texture = uint32_t( texture ),
			.m_ApplyGammaCurve = 1,
			.m_ApplyTonemapping = 1,
		};

		commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );

		commandBuffer->setCullMode( vk::CullModeFlagBits::eFront );
		commandBuffer->draw( 3, 1, 0, 0 );

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );
	}

	// -------------------
	// RT Reflections Pass
	// -------------------
	RaytracedReflectionsPass::RaytracedReflectionsPass( const Viewport& viewport ) : BaseRenderPass( viewport, "RT Reflections Pass" ) {}

	void RaytracedReflectionsPass::CreatePassResources( Device& device ) {
	
	}
	
	void RaytracedReflectionsPass::Dispatch( CommandBuffer& commandBuffer, Device& device ) { 
	
	}

	// ----------
	// Bloom Pass
	// ----------
	BloomPass::BloomPass( const Viewport& viewport ) : BaseRenderPass( viewport, "Bloom Pass" ) {}
	
	void BloomPass::CreatePassResources( Device& device ) { 
		
	}
	
	void BloomPass::Dispatch( CommandBuffer& commandBuffer, Device& device, ImageHandle lightingOutput ) { 
		
	}

	// -------------
	// Skinning Pass
	// -------------
	SkinningPass::SkinningPass( const Viewport& viewport ) : BaseRenderPass( viewport, "Skinning Pass" ) {}
	
	void SkinningPass::CreatePassResources( Device& device ) { 
		 m_Pipeline = m_ComputePipelineBuilder.SetPipelineLayout( device.GetGlobalPipelineLayout() )
	 		.SetShaderBlob( { g_EngineShaders.SkinningShader, vk::ShaderStageFlagBits::eCompute } )
			.Build( device );
	}

	void SkinningPass::Dispatch( CommandBuffer& commandBuffer, Device& device, Scene& scene ) { 

	}
}