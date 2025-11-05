#include "Pch.hpp"
#include "Engine.hpp"

#include "OBJImporter.hpp"
#include "GLTFImporter.hpp"

#include "Device.hpp"

#include "Input.hpp"
#include "RenderPasses.hpp"

namespace Boundless {
	EngineShaders g_EngineShaders = {};

	Engine::Engine( GLFWwindow* window ) : m_GlfwWindow( window ) {
		m_WindowHandle = glfwGetWin32Window( m_GlfwWindow );
		m_Device	   = std::make_unique<Device>( m_WindowHandle );

		// Compile shaders.
		g_EngineShaders.GBufferPixelShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\GBufferPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.GBufferVertexShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\GBufferVS.hlsl", ShaderType::VertexShader );
		g_EngineShaders.GBufferDebugPixelShader   = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\GBufferDebugPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.LightingPixelShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\LightingPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.FullscreenBlitPixelShader = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\BlitPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.CompositePixelShader	  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\ComposePS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.FullscreenTriVertexShader = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\FullscreenVS.hlsl", ShaderType::VertexShader );
		g_EngineShaders.BrdfGenPixelShader		  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\IBL\\BRDFGenPS.hlsl", ShaderType::PixelShader );
		g_EngineShaders.SkinningShader			  = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\SkinningCS.hlsl", ShaderType::ComputeShader );
		
		// Create Swapchain...
		OnResize();

		// Create Sync Objects...
		vk::SemaphoreCreateInfo semaphoreCreateInfo = {};
		vk::FenceCreateInfo fenceCreateInfo = { vk::FenceCreateFlagBits::eSignaled };

		const vk::Device& device = m_Device->GetDevice();

		for ( auto i = 0; i < MaxFramesInFlight; i++ ) {
			FrameData& fd = m_FrameData[ i ];			
			fd.m_CommandPool			 = device.createCommandPool( vk::CommandPoolCreateInfo{ vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_Device->GetQueueIndex() } );
			fd.m_CommandBuffer			 = CommandBuffer( *m_Device, fd.m_CommandPool );
			fd.m_ImageAvailableSemaphore = device.createSemaphore( semaphoreCreateInfo );
			fd.m_InFlightFence		     = device.createFence( fenceCreateInfo );
		}

		m_SwapchainRenderFinishedSemaphores.resize( m_SwapchainImages.size() );
		for ( auto i = 0; i < m_SwapchainRenderFinishedSemaphores.size(); i++ ) {
			m_SwapchainRenderFinishedSemaphores[ i ] = device.createSemaphore( semaphoreCreateInfo );
		}

		m_FrameConstantsBuffer = m_Device->CreateBuffer( Buffer::Desc{
				.m_Size = sizeof( FrameConstants ),
				.m_Usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO,
				.m_Mappable = true
			}
		);

		// m_ComputeFinishedSemaphore = device.createSemaphore( semaphoreCreateInfo );
		// m_ComputeInFlightFence = device.createFence( fenceCreateInfo );

		m_FullscreenPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.FullscreenBlitPixelShader, vk::ShaderStageFlagBits::eFragment }, { g_EngineShaders.FullscreenTriVertexShader, vk::ShaderStageFlagBits::eVertex } } )
			.SetColorAttachmentFormats( m_SwapchainImageFormat )
			.SetPipelineLayout( m_Device->GetGlobalPipelineLayout() )
			.Build( *m_Device );

		const auto& extents = m_SwapchainExtents;

		Camera defaultCamera = Camera::StationaryLookAtCamera(  { 0.f, 0.f, 5.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, 90.f, extents.width / float( extents.height ), 0.1f );

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
		const vk::Device& device = m_Device->GetDevice();
		const vk::CommandPool& commandPool = m_Device->GetCommandPool();

		device.waitIdle();
		device.destroyCommandPool( commandPool );

		DestroySwapchain();

		for( FrameData& frameData : m_FrameData ) {
			device.destroyCommandPool( frameData.m_CommandPool );
			device.destroySemaphore( frameData.m_ImageAvailableSemaphore );
			device.destroyFence( frameData.m_InFlightFence );
		}
		
		for ( vk::Semaphore& semaphore : m_SwapchainRenderFinishedSemaphores )
			device.destroySemaphore( semaphore );
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
		vk::Format brdfFormat = vk::Format::eR16G16Sfloat;
		const int lutDim = 512;

		Image::Desc brdfDesc = Image::Desc{
				.m_Width = lutDim,
				.m_Height = lutDim,
				.m_Format = brdfFormat,
				.m_Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		};

		ImageHandle brdfTex = m_Device->CreateImage( brdfDesc );
		m_BrdfLut = m_Device->CreateImageView( brdfTex, brdfDesc );

		vk::PipelineLayout brdfLutGenPipelineLayout = PipelineLayoutBuilder()
			.SetDescriptorSets( { m_Device->GetGlobalResourceLayout() } )
			.Build( *m_Device );

		vk::Pipeline brdfLutGenPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { g_EngineShaders.BrdfGenPixelShader, vk::ShaderStageFlagBits::eFragment }, { g_EngineShaders.FullscreenTriVertexShader, vk::ShaderStageFlagBits::eVertex } } )
			.SetColorAttachmentFormats( brdfFormat )
			.SetPipelineLayout( brdfLutGenPipelineLayout )
			.Build( *m_Device );

		CommandBuffer commandBuffer = CommandBuffer( *m_Device );
		commandBuffer.Begin();

		vk::ClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
		vk::RenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( m_Device->GetImage( m_BrdfLut ), &clearValue, vk::ImageLayout::eAttachmentOptimal );
		vk::RenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( VkExtent2D{ lutDim, lutDim }, &colorAttachment, nullptr );

		commandBuffer.BeginRendering(renderingInfo);
		commandBuffer.SetScissorAndViewport( m_Device->GetImage( brdfTex ) );
		commandBuffer.BindPipeline(brdfLutGenPipeline);

		commandBuffer->setCullMode( vk::CullModeFlagBits::eFront );
		commandBuffer->setFrontFace( vk::FrontFace::eCounterClockwise );
		commandBuffer->draw( 3, 1, 0, 0 );

		commandBuffer.EndRendering();
		commandBuffer.ImageBarrier( 
			m_Device->GetImage( brdfTex ),
			vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers }
		);

		commandBuffer.End();
		commandBuffer.Submit( m_Device->GetQueue() );

		const vk::Device& device = m_Device->GetDevice();
		device.destroyPipelineLayout( brdfLutGenPipelineLayout );
		device.destroyPipeline( brdfLutGenPipeline );
	}

	void Engine::DestroySwapchain() {
		const vk::Device& device = m_Device->GetDevice();

		device.waitIdle();

		for( const vk::ImageView& imageView : m_SwapchainImageViews )
			device.destroyImageView( imageView );

		m_SwapchainImages.clear();
		m_SwapchainImageViews.clear();

		device.destroySwapchainKHR( m_Swapchain );
	}

	void Engine::Update( float deltaTime ) {
		auto& mainCamera = m_Scene.GetMainCamera();
		mainCamera.Update( deltaTime );

		m_Scene.UpdateTransforms();
	}

	void Engine::Render( float deltaTime ) {
		const vk::Device& device = m_Device->GetDevice();

		if ( m_ResizeRequested ) {
			device.waitIdle();

			OnResize();
			
			m_ResizeRequested = false;
		}

		FrameData& currentFrame = GetCurrentFrame();

		while ( vk::Result::eTimeout == device.waitForFences( currentFrame.m_InFlightFence, vk::True, std::numeric_limits<uint64_t>::max() ) )
			_mm_pause();

		device.resetFences( currentFrame.m_InFlightFence );
		
		try {
			const auto [ result, value ] = device.acquireNextImageKHR( m_Swapchain, std::numeric_limits<uint64_t>::max(), currentFrame.m_ImageAvailableSemaphore, VK_NULL_HANDLE );
			m_CurrentImageIndex = value;
		} catch ( vk::OutOfDateKHRError error ) {
			m_ResizeRequested = true;
			return;
		}

		CommandBuffer& commandBuffer = GetCommandBuffer();
		commandBuffer->reset();
		
		commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
		{
			commandBuffer.BindDefaults( *m_Device );

			static bool rebuiltTLAS;
			if( !rebuiltTLAS ) {
				m_Scene.BuildTLAS( *m_Device, commandBuffer );
				rebuiltTLAS = true;
			}

			const auto& camera = m_Scene.GetMainCamera();

			m_FrameConstants.m_CameraViewProjectionMatrix = camera.GetViewProjectionMatrix();
			m_FrameConstants.m_CameraInvViewProjectionMatrix = glm::inverse( camera.GetViewProjectionMatrix() );
			m_FrameConstants.m_CameraPosition = camera.GetInvViewMatrix()[ 3 ];
			m_FrameConstants.m_SunColor = { 1.f, 1.f, 1.f, 1.f };
			m_FrameConstants.m_SunDirection = glm::normalize( glm::vec4{ 0.25f, -0.9f, 0.f, 1.f } );
			m_FrameConstants.m_IblTextures = { m_DiffuseIbl, m_SpecularIbl, m_BrdfLut, 0 };

			Buffer& frameConstantsBuffer = m_Device->GetBuffer( m_FrameConstantsBuffer );
			frameConstantsBuffer.Patch( &m_FrameConstants, sizeof( m_FrameConstants ) );

			// GBuffer.
			GBufferOutput gbufferOutput = m_GBuffer->Render( commandBuffer, *m_Device, m_FrameConstantsBuffer, m_Scene );

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
				m_Lighting->Render( commandBuffer, *m_Device, m_FrameConstantsBuffer, m_Scene, gbufferOutput );
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
		vk::CommandBufferSubmitInfo cmdInfo = vk::CommandBufferSubmitInfo( commandBuffer );
		vk::SemaphoreSubmitInfo waitInfo    = vk::SemaphoreSubmitInfo( currentFrame.m_ImageAvailableSemaphore, 1, vk::PipelineStageFlagBits2::eColorAttachmentOutput, 0 );
		vk::SemaphoreSubmitInfo signalInfo  = vk::SemaphoreSubmitInfo( m_SwapchainRenderFinishedSemaphores[ m_CurrentImageIndex ], 1, vk::PipelineStageFlagBits2::eAllGraphics, 0 );

		vk::SubmitInfo2 submitInfo =  {};
		submitInfo.setCommandBufferInfos( cmdInfo )
			.setWaitSemaphoreInfos( waitInfo )
			.setSignalSemaphoreInfos( signalInfo );

		const vk::Queue& queue = m_Device->GetQueue();
		queue.submit2( submitInfo, currentFrame.m_InFlightFence );

		vk::PresentInfoKHR presentInfo = {};
		presentInfo.setSwapchains( m_Swapchain )
			.setWaitSemaphores( m_SwapchainRenderFinishedSemaphores[ m_CurrentImageIndex ] )
			.setPImageIndices( &m_CurrentImageIndex );

		try {
			vk::Result result = queue.presentKHR(presentInfo);
		}
		catch ( vk::OutOfDateKHRError error )
		{
			m_ResizeRequested = true;
		}

		m_CurrentFrame = ( m_CurrentFrame + 1 ) % MaxFramesInFlight;
	}

	void Engine::OnResize() {
		const vk::Device& device = m_Device->GetDevice();

		if( m_Swapchain != VK_NULL_HANDLE) {
			DestroySwapchain();
		}

		m_Swapchain = VkUtil::CreateSwapchain( device, m_Device->GetPhysicalDevice(), m_Device->GetSurface(), m_WindowHandle, m_SwapchainExtents, m_SwapchainImageFormat );

		VkUtil::CreateSwapchainImages( device, m_Swapchain, m_SwapchainImageFormat, m_SwapchainImages, m_SwapchainImageViews );

		RecompilePasses();
	}

	void Engine::RenderFinalPass( CommandBuffer& commandBuffer, ImageHandle texture ) {
		vk::Image currentImage = m_SwapchainImages[ m_CurrentImageIndex ];
		vk::ImageView currentView = m_SwapchainImageViews[ m_CurrentImageIndex ];

		vk::ClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
		vk::RenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( currentView, &clearValue, vk::ImageLayout::eAttachmentOptimal );
		vk::RenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( m_SwapchainExtents, &colorAttachment, nullptr );

		commandBuffer.BeginRendering(renderingInfo);
		commandBuffer.SetScissorAndViewport( float( m_SwapchainExtents.width ), float( m_SwapchainExtents.height ), 0.f, 0.f, 0.f, 1.f, false );
		commandBuffer.BindPipeline( m_FullscreenPipeline );

		BlitPushConstants pc = { uint32_t( texture ) };
		commandBuffer.BindPushConstants( *m_Device, &pc, sizeof( pc ) );
		
		commandBuffer->setCullMode( vk::CullModeFlagBits::eFront );
		commandBuffer->draw(3, 1, 0, 0 );

		commandBuffer.EndRendering();
		commandBuffer.ImageBarrier(
			currentImage,
			vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
			vk::AccessFlags{},
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eBottomOfPipe, 
			vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers }
		);
	}
}