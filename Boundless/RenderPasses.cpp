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
				.m_Format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.m_Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 
			} );

		DepthTarget( Image::Desc {
				.m_Width = viewport.Size.width,
				.m_Height = viewport.Size.height,
				.m_Format = VK_FORMAT_D32_SFLOAT,
				.m_Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			} );
	}

	void GBufferPass::CreatePassResources( Device& device ) {
		BaseRenderPass::CreatePassResources( device );
		
		VkDevice deviceHandle = device.GetDevice();
		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.GBufferPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.GBufferVertexShader , VK_SHADER_STAGE_VERTEX_BIT } } )
			.Build( deviceHandle );
	}

	void GBufferPass::Render( CommandBuffer& commandBuffer, Device& device, Scene& scene ) {
		BeginRendering( commandBuffer, device );
		
		commandBuffer.SetScissorAndViewport(m_Viewport);
		commandBuffer.BindPipeline(m_Pipeline);

		vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_BACK_BIT );

		auto& registry = scene.GetRegistry();

		for ( entt::entity entity : registry.group<Mesh>() ) {
			const Mesh& mesh = registry.get<Mesh>( entity );

			// TODO: Make transforms not optional... They should be the first component on every entity.
			glm::mat4 modelMatrix( 1.f );

			// if ( Transform* transform = registry.try_get<Transform>( entity ) ) {
			// 	modelMatrix = transform->GetWorldTransform();
			// }
			
			VkDeviceAddress sceneUniforms  = device.GetBuffer( scene.GetUniformBuffer() ).GetDeviceAddress();
			VkDeviceAddress sceneMaterials = device.GetBuffer( scene.GetMaterialBuffer() ).GetDeviceAddress();
			VkDeviceAddress vertexBuffer   = device.GetBuffer( mesh.m_VertexBuffer ).GetDeviceAddress();
			uint32_t materialIndex		   = mesh.m_Material;

			GBufferPushConstants pc = {
				sceneUniforms,
				sceneMaterials,
				vertexBuffer,
				materialIndex
			};

			commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );

			if ( !mesh.m_Indices.empty() ) {
				Buffer& indexBuffer = device.GetBuffer( mesh.m_IndexBuffer );
				commandBuffer.BindIndexBuffer( indexBuffer );

				vkCmdDrawIndexed( commandBuffer, uint32_t( mesh.m_Indices.size() ), 1, 0, 0, 0 );
			} else {
				vkCmdDraw( commandBuffer, uint32_t( mesh.m_Vertices.size() ), 1, 0, 0 );
			}
		}

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );
	}

	// ------------------
	// GBuffer Debug Pass
	// ------------------
	GBufferDebugPass::GBufferDebugPass( const Viewport& viewport ) : BaseRenderPass( viewport, "GBuffer Debug Pass" ) { 
		RenderTarget( Image::Desc{
			.m_Width = viewport.Size.width,
			.m_Height = viewport.Size.height,
			.m_Format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.m_Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		} );
	}
	
	void GBufferDebugPass::CreatePassResources( Device& device ) { 
		BaseRenderPass::CreatePassResources(device);

		VkDevice deviceHandle = device.GetDevice();
		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.GBufferDebugPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.Build( deviceHandle );
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

		vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_FRONT_BIT );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );

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
			.m_Format = VK_FORMAT_R8G8B8A8_UNORM,
			.m_Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		} );
	}

	void LightingPass::CreatePassResources( Device& device ) { 
		BaseRenderPass::CreatePassResources( device );

		VkDevice deviceHandle = device.GetDevice();
		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.LightingPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.Build( deviceHandle );
	}

	void LightingPass::Render( CommandBuffer& commandBuffer, Device& device, Scene& scene, ImageHandle gbufferTexture, ImageHandle depthTexture ) { 
		BeginRendering( commandBuffer, device );

		commandBuffer.SetScissorAndViewport( float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );
		commandBuffer.BindPipeline( m_Pipeline );

		VkDeviceAddress sceneUniforms = device.GetBuffer( scene.GetUniformBuffer() ).GetDeviceAddress();

		LightingPushConstants pc = {
			.m_SceneBuffer = sceneUniforms,
			.m_GBufferTexture = uint32_t( gbufferTexture ),
			.m_GBufferDepthTexture = uint32_t( depthTexture ),
		};

		commandBuffer.BindPushConstants( device, &pc, sizeof( pc ) );

		vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_FRONT_BIT );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );

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
			.m_Format = VK_FORMAT_R8G8B8A8_UNORM,
			.m_Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		} );
	}

	void CompositePass::CreatePassResources( Device& device ) { 
		BaseRenderPass::CreatePassResources(device);
	
		VkDevice deviceHandle = device.GetDevice();
		m_Pipeline = m_PipelineBuilder
			.SetShaderBlobs( { { g_EngineShaders.CompositePixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.Build( deviceHandle );
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

		vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_FRONT_BIT );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );
	}
}