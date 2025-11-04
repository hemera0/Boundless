#include "Pch.hpp"
#include "Engine.hpp"

#include "OBJImporter.hpp"
#include "GLTFImporter.hpp"

#include "Device.hpp"

#include "Input.hpp"
#include "RenderPasses.hpp"

namespace Boundless {
	EngineShaders g_EngineShaders = {};

	static void MouseCallback( GLFWwindow* window, double xpos, double ypos ) {
		g_Input->SetMousePos( glm::vec2{ xpos, ypos } );
	}

	static void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods ) {
		g_Input->SetKeyState( key, action );
	}

	Engine::Engine() {
		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

		m_GlfwWindow = glfwCreateWindow( 1280, 720, "Boundless", nullptr, nullptr );
		if ( !m_GlfwWindow ) {
			printf( "Failed to create engine window\n" );
			return;
		}

		glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback( m_GlfwWindow, &MouseCallback);
		glfwSetKeyCallback( m_GlfwWindow, &KeyCallback);
		glfwSetWindowUserPointer( m_GlfwWindow, this );

		m_WindowHandle = glfwGetWin32Window( m_GlfwWindow );
		m_Device	   = std::make_unique<Device>( m_WindowHandle );

		for ( auto i = 0; i < m_CommandBuffers.size(); i++ )
			m_CommandBuffers[ i ] = CommandBuffer( *m_Device );

		// Compile shaders.
		g_EngineShaders.GBufferPixelShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\GBufferPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.GBufferVertexShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\GBufferVS.hlsl", ShaderType::VertexShader );
		g_EngineShaders.GBufferDebugPixelShader   = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\GBufferDebugPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.LightingPixelShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\LightingPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.FullscreenBlitPixelShader = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\BlitPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.CompositePixelShader	  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\ComposePS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.FullscreenTriVertexShader = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\FullscreenVS.hlsl", ShaderType::VertexShader );
		g_EngineShaders.BrdfGenPixelShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\IBL\\BRDFGenPS.hlsl", ShaderType::PixelShader );

		// Create Swapchain...
		OnResize();

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

		// This is temporary...
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice();
		VkSurfaceKHR surface = m_Device->GetSurface();

		const auto imageFormat = VkUtil::SwapchainGetImageFormat( physicalDevice, surface );

		m_FullscreenPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.FullscreenBlitPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetColorAttachmentFormats( { imageFormat } )
			.SetPipelineLayout( m_Device->GetGlobalPipelineLayout() )
			.Build( device );

		const auto& extents = m_SwapchainExtents;

		auto defaultCamera = Camera::StationaryLookAtCamera( 
			{ 0.f, 0.f, 5.f }, 
			{ 0.f, 0.f, 0.f }, 
			{ 0.f, 1.f, 0.f }, 
			90.f, 
			extents.width / float( extents.height ), 
			0.1f 
		);

		m_Scene.SetMainCamera( defaultCamera );

		GLTFImporter gltf( m_Scene );
		if(!gltf.LoadFromFile("..\\Assets\\Models\\Sponza\\Sponza.gltf")) {
			printf("Failed to load gltf file\n");
			return;
		}

		// TODO: move?
		m_DiffuseIbl  = m_Device->LoadKTXImageFromFile( "..\\Assets\\IBL\\Inside\\diffuse.ktx2" ).first;
		m_SpecularIbl = m_Device->LoadKTXImageFromFile( "..\\Assets\\IBL\\Inside\\specular.ktx2" ).first;
		GenerateBrdfLut();

		m_Scene.OnDeviceStart(*m_Device);

		RecompilePasses();
	}

	Engine::~Engine() {
		VkDevice device = m_Device->GetDevice();
		VkCommandPool commandPool = m_Device->GetCommandPool();

		vkDeviceWaitIdle( device );
		vkDestroyCommandPool( device, commandPool, nullptr );

		DestroySwapchain();

		// Destroy sync objects...
		for( VkSemaphore semaphore : m_ImageAvailableSemaphores )
			vkDestroySemaphore( device, semaphore, nullptr );

		for ( VkSemaphore semaphore : m_RenderFinishedSemaphores )
			vkDestroySemaphore( device, semaphore, nullptr );

		for( VkFence fence : m_InFlightFences )
			vkDestroyFence( device, fence, nullptr );

		vkDestroySemaphore( device, m_ComputeFinishedSemaphore, nullptr );
		vkDestroyFence( device, m_ComputeInFlightFence, nullptr );
	}

	void Engine::Tick( float dt ) {
		Update( dt );
		Render( dt );
	}

	bool Engine::ShouldExit() {
		return glfwWindowShouldClose( m_GlfwWindow );
	}

	void Engine::RecompilePasses() {
		Viewport viewport = { .Size = m_SwapchainExtents };
		
		// Release all old resources.
		for ( BaseRenderPass* renderPass : m_RenderPasses )
			renderPass->ReleasePassResources( *m_Device );

		m_GBuffer	   = std::make_unique<GBufferPass>( viewport );
		m_GBufferDebug = std::make_unique<GBufferDebugPass>( viewport );
		m_Lighting	   = std::make_unique<LightingPass>( viewport );
		m_Composite	   = std::make_unique<CompositePass>( viewport );

		m_RenderPasses.clear();
		m_RenderPasses.push_back( m_GBuffer.get() );
		m_RenderPasses.push_back( m_GBufferDebug.get() );
		m_RenderPasses.push_back( m_Lighting.get() );
		m_RenderPasses.push_back( m_Composite.get() );
	
		for ( BaseRenderPass* renderPass : m_RenderPasses ) {
			renderPass->CreatePassResources( *m_Device );
		}
	}

	void Engine::GenerateBrdfLut() {
		VkFormat brdfFormat = VK_FORMAT_R16G16_SFLOAT;
		const int lutDim = 512;

		Image::Desc brdfDesc = Image::Desc{
				.m_Width = lutDim,
				.m_Height = lutDim,
				.m_Format = brdfFormat,
				.m_Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		};

		ImageHandle brdfTex = m_Device->CreateImage( brdfDesc );
		m_BrdfLut = m_Device->CreateImageView( brdfTex, brdfDesc );

		VkPipelineLayout brdfLutGenPipelineLayout = PipelineLayoutBuilder()
			.SetDescriptorSets( { m_Device->GetGlobalResourceLayout() } )
			.Build( m_Device->GetDevice() );

		VkPipeline brdfLutGenPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.BrdfGenPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT }, { g_EngineShaders.FullscreenTriVertexShader, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetColorAttachmentFormats( { brdfFormat } )
			.SetPipelineLayout( brdfLutGenPipelineLayout )
			.Build( m_Device->GetDevice() );

		CommandBuffer commandBuffer = CommandBuffer( *m_Device );

		commandBuffer.Begin();

		VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
		VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( m_Device->GetImage( m_BrdfLut ), &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
		VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( VkExtent2D{ lutDim, lutDim }, &colorAttachment, nullptr );

		commandBuffer.BeginRendering(&renderingInfo);
		commandBuffer.SetScissorAndViewport( m_Device->GetImage( brdfTex ) );
		commandBuffer.BindPipeline(brdfLutGenPipeline);

		vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_FRONT_BIT );
		vkCmdSetFrontFace( commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE );
		vkCmdDraw(commandBuffer, 3, 1, 0, 0 );

		commandBuffer.EndRendering();
		commandBuffer.ImageBarrier( 
			m_Device->GetImage( brdfTex ),
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		);

		commandBuffer.End();
		commandBuffer.Submit(m_Device->GetGraphicsQueue());

		VkCommandBuffer handle = commandBuffer;
		vkFreeCommandBuffers(m_Device->GetDevice(), m_Device->GetCommandPool(), 1, &handle);
		vkDestroyPipelineLayout( m_Device->GetDevice(), brdfLutGenPipelineLayout, nullptr );
		vkDestroyPipeline(m_Device->GetDevice(), brdfLutGenPipeline, nullptr);
	}

	void Engine::DestroySwapchain() {
		VkDevice device = m_Device->GetDevice();

		vkDeviceWaitIdle( device );

		for( VkImageView imageView : m_SwapchainImageViews )
			vkDestroyImageView( device, imageView, nullptr );

		m_SwapchainImages.clear();
		m_SwapchainImageViews.clear();

		vkDestroySwapchainKHR( device, m_Swapchain, nullptr );
	}

	void Engine::Update( float deltaTime ) {
		auto& mainCamera = m_Scene.GetMainCamera();
		mainCamera.Update( deltaTime );

		m_Scene.UpdateTransforms();
	}

	void Engine::Render( float deltaTime ) {
		VkDevice device = m_Device->GetDevice();

		vkWaitForFences( device, 1, &m_InFlightFences[ m_CurrentFrame ], VK_TRUE, std::numeric_limits<uint64_t>::max() );
		vkResetFences( device, 1, &m_InFlightFences[ m_CurrentFrame ] );

		VkResult result = vkAcquireNextImageKHR( device, m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[ m_CurrentFrame ], VK_NULL_HANDLE, &m_CurrentImageIndex );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
			OnResize();
			return;
		} else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
			printf( "Failed to acquire swap chain image\n" );
		}

		CommandBuffer& commandBuffer = GetCommandBuffer();
		vkResetCommandBuffer( commandBuffer, 0 );
		commandBuffer.Begin();

		{
			commandBuffer.BindDefaults( *m_Device );

			static bool tlasRebuild = true;
			if( tlasRebuild ) {
				m_Scene.BuildTLAS( *m_Device, commandBuffer );
				tlasRebuild = false;
			}

			const auto& camera = m_Scene.GetMainCamera();

			m_FrameConstants.m_CameraViewProjectionMatrix = camera.GetViewProjectionMatrix();
			m_FrameConstants.m_CameraInvViewProjectionMatrix = glm::inverse( camera.GetViewProjectionMatrix() );
			m_FrameConstants.m_CameraPosition = camera.GetInvViewMatrix()[ 3 ];
			
			m_FrameConstants.m_SunColor = { 1.f, 1.f, 1.f, 1.f };
			m_FrameConstants.m_SunDirection = glm::normalize( glm::vec4{ 0.25f, -0.9f, 0.f, 1.f } );
			m_FrameConstants.m_IblTextures = { m_DiffuseIbl, m_SpecularIbl, m_BrdfLut, 0 };

			// TODO: Move this uniform buffer to Engine
			m_Device->GetBuffer( m_Scene.GetUniformBuffer() ).Patch( &m_FrameConstants, sizeof( m_FrameConstants ) );

			// GBuffer.
			{
				m_GBuffer->Render( commandBuffer, *m_Device, m_Scene );
			}

			// RT Shadows.
			{
				
			}

			// RT AO.
			{
				
			}

			// RT Reflections.
			{
			
			}
			
			// Lighting.
			{
				m_Lighting->Render( commandBuffer, *m_Device, m_Scene, m_GBuffer->GetRenderTargetView(), m_GBuffer->GetDepthBufferView() );
			}

			// Bloom.
			{
			
			}

			// Post Processing.
			{
				m_Composite->Render(commandBuffer, *m_Device, m_Lighting->GetRenderTargetView() );
			}

			bool debugGBuffer = false;

			// GBuffer Debug.
			if( debugGBuffer )
			{
				uint32_t gbufferChannel = 1; // 0 - abledo, 1 - normal, 2 - metallic, 3 - roughness, 4 - albedo
				m_GBufferDebug->Render( commandBuffer, *m_Device, m_GBuffer->GetRenderTargetView(), m_GBuffer->GetDepthBufferView(), gbufferChannel );
			}

			// Final Pass.
			{
				ImageHandle outputTexture = m_Composite->GetRenderTargetView();
				if ( debugGBuffer )
					outputTexture = m_GBufferDebug->GetRenderTargetView();

				RenderFinalPass(commandBuffer, outputTexture);	
			}
		}

		commandBuffer.End();

		// Present.
		VkSemaphore waitSemaphores[ ] = { m_ImageAvailableSemaphores[ m_CurrentFrame ] };
		VkPipelineStageFlags waitStages[ ] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

		VkCommandBuffer commandBuffers[ ] = { commandBuffer };

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = _countof( waitSemaphores );
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = _countof( commandBuffers );
		submitInfo.pCommandBuffers = commandBuffers;

		VkSemaphore signalSemaphores[ ] = { m_RenderFinishedSemaphores[ m_CurrentImageIndex ] };
		submitInfo.signalSemaphoreCount = _countof( signalSemaphores );
		submitInfo.pSignalSemaphores = signalSemaphores;

		result = vkQueueSubmit( m_Device->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[ m_CurrentFrame ] );
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
			OnResize();
		} else if ( result != VK_SUCCESS ) {
			printf( "Failed to present swap chain image!" );
		}

		m_CurrentFrame = ( m_CurrentFrame + 1 ) % MaxFramesInFlight;
	}

	void Engine::OnResize() { 
		if( m_Swapchain != VK_NULL_HANDLE) {
			DestroySwapchain();
		}

		m_Swapchain = VkUtil::CreateSwapchain( m_Device->GetDevice(), m_Device->GetPhysicalDevice(), m_Device->GetSurface(), m_WindowHandle, m_SwapchainExtents );

		const auto imageFormat = VkUtil::SwapchainGetImageFormat( m_Device->GetPhysicalDevice(), m_Device->GetSurface() );
		VkUtil::CreateSwapchainImages( m_Device->GetDevice(), m_Swapchain, imageFormat, m_SwapchainImages, m_SwapchainImageViews );

		RecompilePasses();
	}

	void Engine::RenderFinalPass( CommandBuffer& commandBuffer, ImageHandle texture ) {
		VkImage currentImage = m_SwapchainImages[ m_CurrentImageIndex ];
		VkImageView currentView = m_SwapchainImageViews[ m_CurrentImageIndex ];

		VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
		VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( currentView, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
		VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( m_SwapchainExtents, &colorAttachment, nullptr );

		commandBuffer.BeginRendering(&renderingInfo);
		commandBuffer.SetScissorAndViewport( float( m_SwapchainExtents.width ), float( m_SwapchainExtents.height ), 0.f, 0.f, 0.f, 1.f, false );
		commandBuffer.BindPipeline( m_FullscreenPipeline );

		BlitPushConstants pc = { uint32_t( texture ) };
		commandBuffer.BindPushConstants( *m_Device, &pc, sizeof( pc ) );
		
		vkCmdSetCullMode( commandBuffer, VK_CULL_MODE_FRONT_BIT );
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );

		commandBuffer.EndRendering();
		commandBuffer.ImageBarrier(
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