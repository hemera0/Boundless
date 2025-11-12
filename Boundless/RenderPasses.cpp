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

		for ( entt::entity entity : registry.view<Mesh>() ) {
			const Mesh& mesh = registry.get<Mesh>( entity );
			if ( mesh.m_Indices.empty() )
				continue;

			if ( !registry.all_of<Transform>( entity ) )
				continue;

			vk::DeviceAddress vertexBuffer = device.GetBuffer( mesh.m_VertexBuffer ).GetDeviceAddress();
			if ( mesh.m_SkinnedVertexBuffer != BufferHandle::Invalid ) {
				vertexBuffer = device.GetBuffer( mesh.m_SkinnedVertexBuffer ).GetDeviceAddress();
			}

			Transform& transform = registry.get<Transform>( entity );
			glm::mat4 worldTransform = transform.m_WorldTransform;

			vk::DeviceAddress frameConstants = device.GetBuffer( frameConstantsBuffer ).GetDeviceAddress();
			vk::DeviceAddress sceneMaterials = device.GetBuffer( scene.GetMaterialBuffer() ).GetDeviceAddress();
			uint32_t materialIndex		     = mesh.m_Material;

			GBufferPushConstants pc = {
				.m_FrameConstantsBuffer = frameConstants,
				.m_MaterialsBuffer		= sceneMaterials,
				.m_VertexBuffer			= vertexBuffer,
				.m_WorldTransform		= worldTransform,
				.m_MaterialIndex		= materialIndex,
			};

			Buffer& indexBuffer = device.GetBuffer( mesh.m_IndexBuffer );
			commandBuffer.BindIndexBuffer( indexBuffer );
			commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );
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
	SkinningPass::SkinningPass( ) : BaseRenderPass( Viewport(), "Skinning Pass" ) {}
	
	void SkinningPass::CreatePassResources( Device& device ) { 
		 m_Pipeline = m_ComputePipelineBuilder.SetPipelineLayout( device.GetGlobalPipelineLayout() )
	 		.SetShaderBlob( { g_EngineShaders.SkinningShader, vk::ShaderStageFlagBits::eCompute } )
			.Build( device );
	}

	void SkinningPass::Dispatch( CommandBuffer& commandBuffer, Device& device, Scene& scene ) { 
		auto& registry = scene.GetRegistry();

		auto view = registry.view<Mesh>();
		for( auto [ entity, mesh ] : view.each() ) {
			if ( !registry.valid( mesh.m_Skeleton ) || !registry.all_of<Skeleton>( mesh.m_Skeleton ) )
				continue;

			Skeleton& skeleton = registry.get<Skeleton>( mesh.m_Skeleton );

			// Upload bone matrix data.
			Buffer& boneMatrixBuffer = device.GetBuffer( skeleton.m_BoneTransformsBuffer );

			glm::mat4* boneMatricesData = skeleton.m_BoneTransformMatrices.data();
			size_t boneMatricesSize = skeleton.m_BoneTransformMatrices.size() * sizeof( glm::mat4 );

			std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( boneMatricesSize );
			stagingBuffer->Patch( boneMatricesData, boneMatricesSize );

			commandBuffer.CopyBuffer( *stagingBuffer, boneMatrixBuffer, boneMatricesSize );

			SkinningPushConstants pc = {
				.m_VertexBuffer = device.GetBuffer( mesh.m_VertexBuffer ).GetDeviceAddress(),
				.m_SkinnedVertexBuffer = device.GetBuffer( mesh.m_SkinnedVertexBuffer ).GetDeviceAddress(),
				.m_BoneIndicesBuffer = device.GetBuffer( mesh.m_BoneIndexBuffer ).GetDeviceAddress(),
				.m_BoneWeightsBuffer = device.GetBuffer( mesh.m_BoneWeightBuffer ).GetDeviceAddress(),
				.m_BoneTransformsBuffer = device.GetBuffer( skeleton.m_BoneTransformsBuffer ).GetDeviceAddress(),
				.m_VertexCount = uint32_t( mesh.m_Positions.size() ),
			};
			
			commandBuffer.BindComputePipeline( m_Pipeline );
			commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );
			commandBuffer->dispatch( uint32_t( mesh.m_Positions.size() + 63  / 64 ), 1, 1 );
		}
	}
}