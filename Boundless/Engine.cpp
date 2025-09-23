#include "Pch.hpp"
#include "Engine.hpp"

#include "OBJImporter.hpp"
#include "GLTFImporter.hpp"

#include "Transform.hpp" // TODO: Make components header.
#include "Device.hpp"

#include "Input.hpp"

namespace Boundless {
	Engine* Engine::s_Instance = nullptr;

	struct PushConstants {
		VkDeviceAddress m_SceneBufferAddress;
		VkDeviceAddress m_MaterialsBufferAddress;
		VkDeviceAddress m_VertexBufferAddress;
		// uint32_t m_TLASIndex{};
		// glm::mat4 m_ModelMatrix{};
		uint32_t m_MaterialIndex{};
		char Pad[128 - 32];
	};

	struct FullscreenPushConstants {
		int FrameBufferTexture;
		char Pad[128 - 4];
	};

	// TODO: Fix... This shouldn't be a pointer / Makes no sense.
	std::unique_ptr<Scene> g_MainScene{};

	static void MouseCallback( GLFWwindow* window, double xpos, double ypos ) {
		g_Input->SetMousePos( glm::vec2{ xpos, ypos } );
	}

	static void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods ) {
		g_Input->SetKeyState( key, action );
	}

	void Engine::Create() {
		volkInitialize();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

		m_GlfwWindow = glfwCreateWindow( 1280, 720, "Boundless", nullptr, nullptr );
		if ( !m_GlfwWindow ) {
			printf( "Failed to create engine window\n" );
			return;
		}

		// glfwSetFramebufferSizeCallback(m_GlfwWindow, &FramebufferResizeCallback);
		glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback( m_GlfwWindow, &MouseCallback);
		glfwSetKeyCallback( m_GlfwWindow, &KeyCallback);
		glfwSetWindowUserPointer( m_GlfwWindow, this );

		m_WindowHandle = glfwGetWin32Window( m_GlfwWindow );
		m_Device = std::make_unique<Device>(m_WindowHandle);

		for ( auto i = 0; i < m_CommandBuffers.size(); i++ )
			m_CommandBuffers[ i ] = m_Device->CreateCommandBuffer( );

		// Create Swapchain...
		CreateFrameSizeDependantResources();

		// Create Sync Objects...
		VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_ImageAvailableSemaphores.resize( m_SwapchainImageViews.size() );
		m_RenderFinishedSemaphores.resize( m_SwapchainImageViews.size() );
		m_InFlightFences.resize( m_SwapchainImageViews.size() );

		VkDevice device = m_Device->GetDevice();

		for ( auto i = 0; i < m_SwapchainImageViews.size(); i++ ) {
			if ( vkCreateSemaphore( device, &semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateSemaphore( device, &semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateFence( device, &fenceCreateInfo, nullptr, &m_InFlightFences[ i ] ) != VK_SUCCESS ) {
				printf( "Failed to create VkSemaphore objects or VkFence\n" );
				return;
			}
		}

		if ( vkCreateSemaphore( device, &semaphoreCreateInfo, nullptr, &m_ComputeFinishedSemaphore ) != VK_SUCCESS ||
			vkCreateFence( device, &fenceCreateInfo, nullptr, &m_ComputeInFlightFence ) != VK_SUCCESS ) {
			printf( "Failed to create compute VkSemaphore or VkFence\n" );
			return;
		}

		auto psBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\TestPS.hlsl", ShaderType::PixelShader );
		auto vsBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\TestVS.hlsl", ShaderType::VertexShader );

		// This is temporary...
		m_DefaultPipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ) } } )
			.SetDescriptorSets( { m_Device->GetTexturePoolLayout() } )
			.Build( device );

		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice();
		VkSurfaceKHR surface = m_Device->GetSurface();

		const auto depthFormat = VkUtil::PhysicalDeviceFindDepthFormat( physicalDevice );
		const auto imageFormat = VkUtil::SwapchainGetImageFormat( physicalDevice, surface );

		auto MSAAState = VkUtil::PipelineDefaultMultiSampleState();
		MSAAState.rasterizationSamples = VK_SAMPLE_COUNT_8_BIT;

		m_DefaultPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { psBlob, VK_SHADER_STAGE_FRAGMENT_BIT }, { vsBlob, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetColorAttachmentFormats( { VK_FORMAT_R32G32B32A32_SFLOAT } )
			.SetDepthAttachmentFormat( depthFormat )
			.SetPipelineLayout( m_DefaultPipelineLayout )
			.SetMultisampleState( MSAAState )
			.Build( device, m_Swapchain );

		auto fullscreenPsBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\ComposePS.hlsl", ShaderType::PixelShader );
		auto fullscreenVsBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\FullscreenVS.hlsl", ShaderType::VertexShader );

		m_FullscreenPipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( FullscreenPushConstants ) } } )
			.SetDescriptorSets( { m_Device->GetTexturePoolLayout() } )
			.Build( device );

		auto fullscreenRasterization = VkUtil::PipelineDefaultRasterizationState();
		fullscreenRasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
		fullscreenRasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		m_FullscreenPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { fullscreenPsBlob, VK_SHADER_STAGE_FRAGMENT_BIT }, { fullscreenVsBlob, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetRasterizationState( fullscreenRasterization )
			.SetColorAttachmentFormats( { imageFormat } )
			.SetPipelineLayout( m_FullscreenPipelineLayout )
			.Build( device, m_Swapchain );

		g_MainScene = std::make_unique<Scene>();

		const auto& extents = m_SwapchainExtents;

		auto defaultCamera = Camera::StationaryLookAtCamera( 
			{ 0.f, 0.f, 5.f }, 
			{ 0.f, 0.f, 0.f }, 
			{ 0.f, 1.f, 0.f }, 
			90.f, 
			extents.width / float( extents.height ), 
			0.1f 
		);

		g_MainScene->SetMainCamera( defaultCamera );

		//OBJImporter importer( *g_MainScene );
		//if ( !importer.LoadFromFile( "..\\Assets\\Models\\suzanne.obj" ) ) {
		//	printf( "Failed to load obj file\n" );
		//	return;
		//}

		GLTFImporter gltf(*g_MainScene);
		if(!gltf.LoadFromFile("..\\Assets\\Models\\Sponza\\Sponza.gltf")) {
			printf("Failed to load gltf file\n");
			return;
		}

		Image diffuseTex = m_Device->LoadKTXImageFromFile( "..\\Assets\\IBL\\Inside\\diffuse.ktx2" );
		Image specularTex = m_Device->LoadKTXImageFromFile( "..\\Assets\\IBL\\Inside\\specular.ktx2" );

		VkSampler sampler = m_Device->CreateSampler( 
			SamplerDesc {
				.m_MinFilter = VK_FILTER_LINEAR,
				.m_MagFilter = VK_FILTER_LINEAR,
				.m_WrapS = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_WrapT = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			});

		m_DiffuseIbl = m_Device->CreateTexture( diffuseTex.GetView(), sampler );
		m_SpecularIbl = m_Device->CreateTexture( specularTex.GetView(), sampler );

		GenerateBrdfLut();

		g_MainScene->OnDeviceStart(m_Device);

		printf( "Finished Initializing\n" );
	}

	void Engine::Destroy() {
		VkDevice device = m_Device->GetDevice();
		VkCommandPool commandPool = m_Device->GetCommandPool();

		vkDeviceWaitIdle( device );
		vkFreeCommandBuffers( device, commandPool, uint32_t( m_CommandBuffers.size() ), m_CommandBuffers.data() );

		// Destroy pipelines...
		vkDestroyPipelineLayout( device, m_DefaultPipelineLayout, nullptr );
		vkDestroyPipeline( device, m_DefaultPipeline, nullptr );

		DestroyFrameSizeDependantResources();

		// Destroy sync objects...
		std::for_each( m_ImageAvailableSemaphores.begin(), m_ImageAvailableSemaphores.end(),
			[ & ]( auto&& semaphore )
		{
			vkDestroySemaphore( device, semaphore, nullptr );
		}
		);

		std::for_each( m_RenderFinishedSemaphores.begin(), m_RenderFinishedSemaphores.end(),
			[ & ]( auto&& semaphore )
		{
			vkDestroySemaphore( device, semaphore, nullptr );
		}
		);

		std::for_each( m_InFlightFences.begin(), m_InFlightFences.end(),
			[ & ]( auto&& fence )
		{
			vkDestroyFence( device, fence, nullptr );
		}
		);

		vkDestroySemaphore( device, m_ComputeFinishedSemaphore, nullptr );
		vkDestroyFence( device, m_ComputeInFlightFence, nullptr );
	}

	void Engine::Tick( float dt ) {
		Update( dt );
		Render();
	}

	bool Engine::ShouldExit() {
		return glfwWindowShouldClose( m_GlfwWindow );
	}

	void Engine::BeginFrame() {
		VkDevice device = m_Device->GetDevice();

		vkWaitForFences( device, 1, &m_InFlightFences[ m_CurrentFrame ], VK_TRUE, std::numeric_limits<uint64_t>::max() );
		vkResetFences( device, 1, &m_InFlightFences[ m_CurrentFrame ] );

		const auto result = vkAcquireNextImageKHR( device, m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[ m_CurrentFrame ], VK_NULL_HANDLE, &m_CurrentImageIndex );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
			DestroyFrameSizeDependantResources();
			CreateFrameSizeDependantResources();
			return;
		} else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
			printf( "Failed to acquire swap chain image\n" );
		}

		vkResetCommandBuffer( m_CommandBuffers[ m_CurrentFrame ], 0 );
		VkUtil::CommandBufferBegin( m_CommandBuffers[ m_CurrentFrame ] );
	}

	void Engine::EndFrame() {
		VkUtil::CommandBufferEnd( m_CommandBuffers[ m_CurrentFrame ] );

		VkSemaphore waitSemaphores[ ] = { m_ImageAvailableSemaphores[ m_CurrentFrame ] };
		VkPipelineStageFlags waitStages[ ] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

		VkCommandBuffer commandBuffers[ ] = { m_CommandBuffers[ m_CurrentFrame ] };

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = _countof( waitSemaphores );
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = _countof( commandBuffers );
		submitInfo.pCommandBuffers = commandBuffers;

		VkSemaphore signalSemaphores[ ] = { m_RenderFinishedSemaphores[ m_CurrentImageIndex ] };
		submitInfo.signalSemaphoreCount = _countof( signalSemaphores );
		submitInfo.pSignalSemaphores = signalSemaphores;

		VkResult result = vkQueueSubmit( m_Device->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[ m_CurrentFrame ] );
		if ( result != VK_SUCCESS ) {
			printf( "Failed to submit draw command buffer: %d\n", result );
		}

		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = _countof( signalSemaphores );
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[ ] = { m_Swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &m_CurrentImageIndex;

		result = vkQueuePresentKHR( m_Device->GetPresentQueue(), &presentInfo );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
			DestroyFrameSizeDependantResources();
			CreateFrameSizeDependantResources();
		} else if ( result != VK_SUCCESS ) {
			printf( "Failed to present swap chain image!" );
		}

		m_CurrentFrame = ( m_CurrentFrame + 1 ) % MaxFramesInFlight;
	}

	void Engine::CreateFrameSizeDependantResources() {
		m_Swapchain = VkUtil::CreateSwapchain( m_Device->GetDevice(), m_Device->GetPhysicalDevice(), m_Device->GetSurface(), m_WindowHandle, m_SwapchainExtents );

		const auto imageFormat = VkUtil::SwapchainGetImageFormat( m_Device->GetPhysicalDevice(), m_Device->GetSurface() );
		VkUtil::CreateSwapchainImages( m_Device->GetDevice(), m_Swapchain, imageFormat, m_SwapchainImages, m_SwapchainImageViews );

		const auto depthFormat = VkUtil::PhysicalDeviceFindDepthFormat( m_Device->GetPhysicalDevice() );
		for(auto i = 0; i < m_DepthImages.size(); i++) {
			m_DepthImages[i] = m_Device->CreateImage( Image::Desc{
					.m_Type = VK_IMAGE_TYPE_2D,
					.m_Width = m_SwapchainExtents.width,
					.m_Height = m_SwapchainExtents.height,
					.m_Levels = 1,
					.m_Format = depthFormat,
					.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
					.m_Samples = VK_SAMPLE_COUNT_8_BIT,
					.m_Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				} );

			VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			textureView.image = m_DepthImages[ i ];
			textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			textureView.format = depthFormat;
			textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
			textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

			m_DepthImageViews[i] = VkUtil::CreateImageView(m_Device->GetDevice(), m_DepthImages[i], &textureView);
		}

		VkFormat hdrFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

		m_HDRFrameBuffer = m_Device->CreateImage(Image::Desc{
			.m_Type = VK_IMAGE_TYPE_2D,
			.m_Width = m_SwapchainExtents.width,
			.m_Height = m_SwapchainExtents.height,
			.m_Levels = 1,
			.m_Format = hdrFormat,
			.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
			.m_Samples = VK_SAMPLE_COUNT_1_BIT,
			.m_Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		});

		VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		textureView.format = hdrFormat;
		textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		m_HDRFrameBuffer.m_ImageView = VkUtil::CreateImageView( m_Device->GetDevice(), m_HDRFrameBuffer, &textureView );

		VkSampler sampler = m_Device->CreateSampler(
			SamplerDesc{
				.m_MinFilter = VK_FILTER_LINEAR,
				.m_MagFilter = VK_FILTER_LINEAR,
				.m_WrapS = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_WrapT = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			} );

		m_HdrHandle = m_Device->CreateTexture( m_HDRFrameBuffer.m_ImageView, sampler );

		m_MSAAFrameBuffer = m_Device->CreateImage( Image::Desc{
			.m_Type = VK_IMAGE_TYPE_2D,
			.m_Width = m_SwapchainExtents.width,
			.m_Height = m_SwapchainExtents.height,
			.m_Levels = 1,
			.m_Format = hdrFormat,
			.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
			.m_Samples = VK_SAMPLE_COUNT_8_BIT,
			.m_Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			} );

		m_MSAAFrameBuffer.m_ImageView = VkUtil::CreateImageView( m_Device->GetDevice(), m_MSAAFrameBuffer, &textureView );
	}

	void Engine::GenerateBrdfLut() {
		VkFormat brdfFormat = VK_FORMAT_R16G16_SFLOAT;
		const int lutDim = 512;

		Image brdfTex = m_Device->CreateImage( Image::Desc {
				.m_Type = VK_IMAGE_TYPE_2D,
				.m_Width = lutDim,
				.m_Height = lutDim,
				.m_Levels = 1,
				.m_Format = brdfFormat,
				.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
				.m_Samples = VK_SAMPLE_COUNT_1_BIT,
				.m_Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			} );
		
		VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		textureView.format = brdfFormat;
		textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		brdfTex.m_ImageView = VkUtil::CreateImageView( m_Device->GetDevice(), brdfTex, &textureView );

		auto psBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\IBL\\BRDFGenPS.hlsl", ShaderType::PixelShader );
		auto vsBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\FullscreenVS.hlsl", ShaderType::VertexShader );

		VkPipelineLayout brdfLutGenPipelineLayout = PipelineLayoutBuilder()
			// .SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( FullscreenPushConstants ) } } )
			.SetDescriptorSets( { m_Device->GetTexturePoolLayout() } )
			.Build( m_Device->GetDevice() );

		auto fullscreenRasterization = VkUtil::PipelineDefaultRasterizationState();
		fullscreenRasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
		fullscreenRasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		VkPipeline brdfLutGenPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { psBlob, VK_SHADER_STAGE_FRAGMENT_BIT }, { vsBlob, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetRasterizationState( fullscreenRasterization )
			.SetColorAttachmentFormats( { brdfFormat } )
			.SetPipelineLayout( brdfLutGenPipelineLayout )
			.Build( m_Device->GetDevice(), m_Swapchain );

		VkCommandBuffer commandBuffer = m_Device->CreateCommandBuffer();

		VkUtil::CommandBufferBegin( commandBuffer );

		VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
		VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( brdfTex.GetView(), &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
		VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( VkExtent2D{ lutDim, lutDim }, &colorAttachment, nullptr );

		VkUtil::CommandBufferBeginRendering( commandBuffer, &renderingInfo );
		VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, float( lutDim ), float(lutDim), 0.f, 0.f, 0.f, 1.f, false );

		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, brdfLutGenPipeline );
		vkCmdDraw(commandBuffer, 3, 1, 0, 0 );

		VkUtil::CommandBufferEndRendering(commandBuffer);

		VkUtil::CommandBufferImageBarrier(
			commandBuffer,
			brdfTex,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		);

		VkUtil::CommandBufferEnd(commandBuffer);
		VkUtil::CommandBufferSubmit(commandBuffer, m_Device->GetGraphicsQueue());
		
		VkSampler sampler = m_Device->CreateSampler(
			SamplerDesc{
				.m_MinFilter = VK_FILTER_LINEAR,
				.m_MagFilter = VK_FILTER_LINEAR,
				.m_WrapS = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_WrapT = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_Anisotropic = false
			} );

		m_BrdfLut = m_Device->CreateTexture( brdfTex.GetView(), sampler );

		vkFreeCommandBuffers(m_Device->GetDevice(), m_Device->GetCommandPool(), 1, &commandBuffer);
	
		vkDestroyPipelineLayout( m_Device->GetDevice(), brdfLutGenPipelineLayout, nullptr );
		vkDestroyPipeline(m_Device->GetDevice(), brdfLutGenPipeline, nullptr);
	}

	void Engine::DestroyFrameSizeDependantResources() {
		VkDevice device = m_Device->GetDevice();

		vkDeviceWaitIdle( device );

		std::for_each( m_SwapchainImageViews.begin(), m_SwapchainImageViews.end(),
			[ & ]( const auto imageView )
		{
			vkDestroyImageView( device, imageView, nullptr );
		});

		std::for_each( m_DepthImageViews.begin(), m_DepthImageViews.end(),
			[ & ]( const auto imageView )
		{
			vkDestroyImageView( device, imageView, nullptr );
		} );

		m_SwapchainImages.clear();
		m_SwapchainImageViews.clear();

		vkDestroySwapchainKHR( device, m_Swapchain, nullptr );
	}

	void Engine::Update( float dt ) {
		auto& mainCamera = g_MainScene->GetMainCamera();
		mainCamera.Update(dt);
	}

	void Engine::Render() {
		BeginFrame();

		{
			const auto& commandBuffer = GetCurrentCommandBuffer();

			static bool tlasRebuild = true;
			if( tlasRebuild ) {
				g_MainScene->BuildTLAS( m_Device, commandBuffer );
				tlasRebuild = false;
			}

			{
				VkImageView depthImageView = m_DepthImageViews[ m_CurrentFrame ];
				VkImageView frameBuffer = m_HDRFrameBuffer.m_ImageView;
				VkImageView msaaFrameBuffer = m_MSAAFrameBuffer.m_ImageView;

				VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
				VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( msaaFrameBuffer, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_RESOLVE_MODE_AVERAGE_BIT, frameBuffer, VK_IMAGE_LAYOUT_GENERAL );
				VkRenderingAttachmentInfo depthAttachment = VkUtil::RenderPassGetDepthAttachmentInfo( depthImageView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
				VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( m_SwapchainExtents, &colorAttachment, &depthAttachment );

				VkUtil::CommandBufferBeginRendering( commandBuffer, &renderingInfo );
				VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, static_cast< float >( m_SwapchainExtents.width ), static_cast< float >( m_SwapchainExtents.height ), 0.f, 0.f, 0.f, 1.f );

				VkDescriptorSet texturePool = m_Device->GetTexturePool();

				vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline );
				vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipelineLayout, 0, 1, &texturePool, 0, nullptr );



				RenderScene( *g_MainScene );

				VkUtil::CommandBufferEndRendering( commandBuffer );

				VkUtil::CommandBufferImageBarrier(
					commandBuffer,
					m_HDRFrameBuffer,
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				);
			}

			{
				VkImage currentImage = m_SwapchainImages[ m_CurrentImageIndex ];
				VkImageView currentView = m_SwapchainImageViews[ m_CurrentImageIndex ];

				VkClearValue clearValue = { { 1.f, 0.1f, 0.1f, 1.f } };
				VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( currentView, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
				VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( m_SwapchainExtents, &colorAttachment, nullptr );

				VkUtil::CommandBufferBeginRendering( commandBuffer, &renderingInfo );
				VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, static_cast< float >( m_SwapchainExtents.width ), static_cast< float >( m_SwapchainExtents.height ), 0.f, 0.f, 0.f, 1.f, false );

				VkDescriptorSet texturePool = m_Device->GetTexturePool();

				// vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_FRONT_BIT );
				// vkCmdSetFrontFace( commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE );
				vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_FullscreenPipeline );
				vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_FullscreenPipelineLayout, 0, 1, &texturePool, 0, nullptr );
				
				FullscreenPushConstants pc = { .FrameBufferTexture = int(m_HdrHandle) };

				vkCmdPushConstants( commandBuffer, m_FullscreenPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( FullscreenPushConstants ), &pc );
				vkCmdDraw( commandBuffer, 3, 1, 0, 0 );

				VkUtil::CommandBufferEndRendering( commandBuffer );
			
				VkUtil::CommandBufferImageBarrier(
					commandBuffer,
					currentImage,
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					0,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
				);
			}
		}

		EndFrame();
	}

	void Engine::RenderScene( Scene& scene ) {
		VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();

		auto& registry = scene.GetRegistry();
		
		const auto& camera = scene.GetMainCamera();

		SceneInfo sceneInfo = {};
		sceneInfo.m_CameraViewProjectionMatrix = camera.GetViewProjectionMatrix();
		sceneInfo.m_CameraPosition = camera.GetInvViewMatrix()[3];
		sceneInfo.m_SunColor = { 1.f, 1.f, 1.f, 1.f };
		sceneInfo.m_SunDirection = glm::normalize( glm::vec4{ 0.25f, -0.9f, 0.f, 1.f } );
		sceneInfo.m_IblTextures = { m_DiffuseIbl, m_SpecularIbl, m_BrdfLut, 0 };

		m_Device->GetBuffer( scene.GetUniformBuffer() )->Patch( &sceneInfo, sizeof( sceneInfo ) );

		for ( entt::entity entity : registry.group<Mesh>() ) {
			const Mesh& mesh = registry.get<Mesh>( entity );

			// TODO: Make transforms not optional... They should be the first component on every entity.
			glm::mat4 modelMatrix( 1.f );

			if ( Transform* transform = registry.try_get<Transform>( entity ) ) {
				modelMatrix = transform->GetWorldTransform();
			} else {
			}
			
			modelMatrix = glm::rotate(glm::mat4(1.f), float(glfwGetTime()), {0.f, 1.f, 0.f});

			uint32_t materialIndex = mesh.m_Material;

			Buffer* vertexBuffer = m_Device->GetBuffer(mesh.m_VertexBuffer);
			Buffer* sceneUniforms = m_Device->GetBuffer( scene.GetUniformBuffer() );
			Buffer* sceneMaterials = m_Device->GetBuffer( scene.GetMaterialBuffer() );

			PushConstants pushConstants = {
				sceneUniforms->GetDeviceAddress(),
				sceneMaterials->GetDeviceAddress(),
				vertexBuffer->GetDeviceAddress(),
				
				// modelMatrix,
				materialIndex
			};

			vkCmdPushConstants( commandBuffer, m_DefaultPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
		
			if(!mesh.m_Indices.empty()) {
				Buffer* indexBuffer = m_Device->GetBuffer(mesh.m_IndexBuffer);
				vkCmdBindIndexBuffer( commandBuffer, *indexBuffer, 0, VK_INDEX_TYPE_UINT32 );
				vkCmdDrawIndexed( commandBuffer, uint32_t( mesh.m_Indices.size() ), 1, 0, 0, 0);
			} else {
				vkCmdDraw( commandBuffer, uint32_t( mesh.m_Vertices.size() ), 1, 0, 0 );
			}
		}
	}
}