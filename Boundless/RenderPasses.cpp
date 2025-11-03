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
		
		m_PipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( GBufferPushConstants ) } } )
			.SetDescriptorSets( { device.GetTexturePoolLayout() } )
			.Build( deviceHandle );

		std::vector<VkFormat> rtFormats = m_RenderTargetDescs | std::views::transform( &Image::Desc::m_Format ) | std::ranges::to<std::vector>();

		m_Pipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.GBufferPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.GBufferVertexShader , VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetColorAttachmentFormats( rtFormats )
			.SetDepthAttachmentFormat( m_DepthDesc.m_Format )
			.SetPipelineLayout( m_PipelineLayout )
			// .SetMultisampleState( MSAAState )
			.Build( deviceHandle );	
	}


	void GBufferPass::Render( VkCommandBuffer commandBuffer, Device& device, Scene& scene ) {
		BeginRendering( commandBuffer, device );
		
		// This is a little ugly I think.
		VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, float( m_Viewport.Size.width ), float( m_Viewport.Size.height ) );

		VkDescriptorSet texturePool = device.GetTexturePool();
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &texturePool, 0, nullptr );

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

			GBufferPushConstants pushConstants = {
				sceneUniforms,
				sceneMaterials,
				vertexBuffer,
				materialIndex
			};

			vkCmdPushConstants( commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( GBufferPushConstants ), &pushConstants );

			if ( !mesh.m_Indices.empty() ) {
				Buffer& indexBuffer = device.GetBuffer( mesh.m_IndexBuffer );
				vkCmdBindIndexBuffer( commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32 );
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

		m_PipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( GBufferDebugPushConstants ) } } )
			.SetDescriptorSets( { device.GetTexturePoolLayout() } )
			.Build( deviceHandle );

		std::vector<VkFormat> rtFormats = m_RenderTargetDescs | std::views::transform( &Image::Desc::m_Format ) | std::ranges::to<std::vector>();

		auto fullscreenRasterization = VkUtil::PipelineDefaultRasterizationState();
		fullscreenRasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
		fullscreenRasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		m_Pipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.GBufferDebugPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetRasterizationState( fullscreenRasterization )
			.SetColorAttachmentFormats( rtFormats )
			.SetPipelineLayout( m_PipelineLayout )
			.Build( deviceHandle );
	}
	
	void GBufferDebugPass::Render( VkCommandBuffer commandBuffer, Device& device, ImageHandle gbufferTexture, ImageHandle depthTexture, uint32_t channelIndex ) {
		BeginRendering( commandBuffer, device );

		VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );

		VkDescriptorSet texturePool = device.GetTexturePool();
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &texturePool, 0, nullptr );

		GBufferDebugPushConstants pc = {
			.m_GBufferTexture = uint32_t( gbufferTexture ),
			.m_DepthTexture = uint32_t( depthTexture ),
			.m_GBufferChannel = channelIndex
		};

		vkCmdPushConstants( commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( GBufferDebugPushConstants ), &pc );
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

		m_PipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( LightingPushConstants ) } } )
			.SetDescriptorSets( { device.GetTexturePoolLayout() } )
			.Build( deviceHandle );

		std::vector<VkFormat> rtFormats = m_RenderTargetDescs | std::views::transform( &Image::Desc::m_Format ) | std::ranges::to<std::vector>();

		auto fullscreenRasterization = VkUtil::PipelineDefaultRasterizationState();
		fullscreenRasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
		fullscreenRasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		m_Pipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.LightingPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetRasterizationState( fullscreenRasterization )
			.SetColorAttachmentFormats( rtFormats )
			.SetPipelineLayout( m_PipelineLayout )
			.Build( deviceHandle );
	}

	void LightingPass::Render( VkCommandBuffer commandBuffer, Device& device, Scene& scene, ImageHandle gbufferTexture, ImageHandle depthTexture ) { 
		BeginRendering( commandBuffer, device );

		VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );

		VkDescriptorSet texturePool = device.GetTexturePool();
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &texturePool, 0, nullptr );

		VkDeviceAddress sceneUniforms = device.GetBuffer( scene.GetUniformBuffer() ).GetDeviceAddress();

		LightingPushConstants pc = {
			.m_SceneBuffer = sceneUniforms,
			.m_GBufferTexture = uint32_t( gbufferTexture ),
			.m_GBufferDepthTexture = uint32_t( depthTexture ),
		};

		vkCmdPushConstants( commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( LightingPushConstants ), &pc );
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

		m_PipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( CompositePushConstants ) } } )
			.SetDescriptorSets( { device.GetTexturePoolLayout() } )
			.Build( deviceHandle );

		std::vector<VkFormat> rtFormats = m_RenderTargetDescs | std::views::transform( &Image::Desc::m_Format ) | std::ranges::to<std::vector>();

		auto fullscreenRasterization = VkUtil::PipelineDefaultRasterizationState();
		fullscreenRasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
		fullscreenRasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		m_Pipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.CompositePixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetRasterizationState( fullscreenRasterization )
			.SetColorAttachmentFormats( rtFormats )
			.SetPipelineLayout( m_PipelineLayout )
			.Build( deviceHandle );
	}
	
	void CompositePass::Render( VkCommandBuffer commandBuffer, Device& device, ImageHandle texture ) { 
		BeginRendering( commandBuffer, device );

		VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, float( m_Viewport.Size.width ), float( m_Viewport.Size.height ), 0.f, 0.f, 0.f, 1.f, false );

		VkDescriptorSet texturePool = device.GetTexturePool();
		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline );
		vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &texturePool, 0, nullptr );

		CompositePushConstants pc = {
			.m_Texture = uint32_t( texture ),
			.m_ApplyGammaCurve = 1,
			.m_ApplyTonemapping = 1,
		};

		vkCmdPushConstants( commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( CompositePushConstants ), &pc );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );

		EndRendering( commandBuffer, device );
		AddExitBarriers( commandBuffer, device );
	}
}