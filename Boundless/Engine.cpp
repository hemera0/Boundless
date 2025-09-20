#include "Pch.hpp"
#include "Engine.hpp"

#include "OBJImporter.hpp"
#include "GLTFImporter.hpp"

#include "Transform.hpp" // TODO: Make components header.
#include "Device.hpp"

namespace Boundless {
	Engine* Engine::s_Instance = nullptr;

	struct alignas(16) PushConstants {
		VkDeviceAddress m_SceneBufferAddress;
		VkDeviceAddress m_MaterialsBufferAddress;
		VkDeviceAddress m_VertexBufferAddress;
		glm::mat4 m_ModelMatrix{};
		uint32_t m_MaterialIndex{};
		char Pad[128 - 96];
	};

	// TODO: Fix... This shouldn't be a pointer / Makes no sense.
	std::unique_ptr<Scene> g_MainScene{};

	void Engine::Create() {
		volkInitialize();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

		m_GlfwWindow = glfwCreateWindow( 1280, 720, "Boundless", nullptr, nullptr );
		if ( !m_GlfwWindow ) {
			printf( "Failed to create engine window\n" );
			return;
		}

		// glfwSetFramebufferSizeCallback(m_EngineGlfwWindow, &FramebufferResizeCallback);
		// glfwSetCursorPosCallback(m_EngineGlfwWindow, &MouseCallback);
		// glfwSetKeyCallback(m_EngineGlfwWindow, &KeyCallback);
		// glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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

		m_DefaultPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { psBlob, VK_SHADER_STAGE_FRAGMENT_BIT }, { vsBlob, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetColorAttachmentFormats( { imageFormat } )
			.SetDepthAttachmentFormat( depthFormat )
			.SetPipelineLayout( m_DefaultPipelineLayout )
			.Build( device, m_Swapchain );



		g_MainScene = std::make_unique<Scene>();

		const auto& extents = m_SwapchainExtents;

		auto defaultCamera = Camera::StationaryLookAtCamera( { 0.f, 0.f, 5.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, 90.f, extents.width / float( extents.height ), 0.01f );

		g_MainScene->SetMainCamera( defaultCamera );

		//OBJImporter importer( *g_MainScene );
		//if ( !importer.LoadFromFile( "..\\Assets\\Models\\suzanne.obj" ) ) {
		//	printf( "Failed to load obj file\n" );
		//	return;
		//}

		GLTFImporter gltf(*g_MainScene);
		if(!gltf.LoadFromFile("..\\Assets\\Models\\Helmet\\DamagedHelmet.gltf")) {
			printf("Failed to load gltf file\n");
			return;
		}

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

	void Engine::Tick() {
		Update();
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
					.m_Width = m_SwapchainExtents.width,
					.m_Height = m_SwapchainExtents.height,
					.m_Levels = 1,
					.m_Format = depthFormat,
					.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
					.m_Samples = VK_SAMPLE_COUNT_1_BIT,
					.m_Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
				} );

			VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			textureView.image = m_DepthImages[ i ];
			textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			textureView.format = depthFormat;
			textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
			textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

			m_DepthImageViews[i] = VkUtil::CreateImageView(m_Device->GetDevice(), m_DepthImages[i], &textureView);
		}
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

	void Engine::Update() {
		auto& mainCamera = g_MainScene->GetMainCamera();
		mainCamera.Update();
	}

	void Engine::Render() {
		BeginFrame();

		{
			VkImage currentImage = m_SwapchainImages[ m_CurrentImageIndex ];
			VkImageView currentView = m_SwapchainImageViews[ m_CurrentImageIndex ];

			VkImage depthImage = m_DepthImages[ m_CurrentFrame ];
			VkImageView depthImageView = m_DepthImageViews[ m_CurrentFrame ];

			VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
			VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( currentView, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
			VkRenderingAttachmentInfo depthAttachment = VkUtil::RenderPassGetDepthAttachmentInfo( depthImageView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );

			const auto& extents = m_SwapchainExtents;

			VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( extents, &colorAttachment, &depthAttachment );

			const auto& commandBuffer = GetCurrentCommandBuffer();

			VkUtil::CommandBufferBeginRendering( commandBuffer, &renderingInfo );
			VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, static_cast< float >( extents.width ), static_cast< float >( extents.height ) );

			VkDescriptorSet texturePool = m_Device->GetTexturePool();

			vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline );
			vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipelineLayout, 0, 1, &texturePool, 0, nullptr );

			RenderScene( *g_MainScene );

			VkUtil::CommandBufferEndRendering( commandBuffer );

			VkUtil::CommandBufferImageBarrier(
				commandBuffer,
				currentImage,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				0,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			);
		}

		EndFrame();
	}

	void Engine::RenderScene( Scene& scene ) {
		VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();

		auto& registry = scene.GetRegistry();
		
		const auto& camera = scene.GetMainCamera();

		SceneInfo sceneInfo = {};
		sceneInfo.m_CameraViewProjectionMatrix = camera.GetViewProjectionMatrix();
		sceneInfo.m_SunColor = { 1.f, 1.f, 1.f, 1.f };
		sceneInfo.m_SunDirection = glm::normalize( glm::vec4{ -1.5f, 0.f, -.075f, 1.f } );

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
				modelMatrix,
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