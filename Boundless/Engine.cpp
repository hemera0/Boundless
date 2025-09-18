#define VOLK_IMPLEMENTATION
#include "Engine.hpp"

#include "OBJImporter.hpp"
#include "GLTFImporter.hpp"

#include "Transform.hpp" // TODO: Make components header.

namespace Boundless {
	Engine* Engine::s_Instance = nullptr;

	struct PushConstants {
		VkDeviceAddress m_SceneBufferAddress;
		VkDeviceAddress m_MaterialsBufferAddress;
		VkDeviceAddress m_VertexBufferAddress;
		// glm::mat4 m_ModelMatrix{};
		uint32_t m_MaterialIndex{};

		char Pad[128 - 32];
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

		uint32_t extensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions( &extensionCount );

		std::vector<const char*> extensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
			// VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		};

		m_Instance = VkUtil::CreateInstance( glfwExtensions, extensionCount );
		if ( m_Instance == VK_NULL_HANDLE )
			return;

		// extensions.pop_back();

		volkLoadInstanceOnly( m_Instance );

		m_PhysicalDevice = VkUtil::GetPhysicalDevice( m_Instance, extensions );
		m_Surface = VkUtil::CreateSurfaceForWindow( m_Instance, m_WindowHandle );
		m_Device = VkUtil::CreateLogicalDevice( m_Instance, m_PhysicalDevice, m_Surface, extensions );

		volkLoadDevice(m_Device);

		const auto& queueFamilyIndices = VkUtil::FindQueueFamilyIndices( m_Surface, m_PhysicalDevice );
		m_GraphicsQueue = VkUtil::DeviceGetQueueByIndex( m_Device, queueFamilyIndices.m_GraphicsFamilyIndex );
		m_PresentQueue = VkUtil::DeviceGetQueueByIndex( m_Device, queueFamilyIndices.m_PresentFamilyIndex );
		m_ComputeQueue = VkUtil::DeviceGetQueueByIndex( m_Device, queueFamilyIndices.m_ComputeFamilyIndex );

		VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolCreateInfo.queueFamilyIndex = queueFamilyIndices.m_GraphicsFamilyIndex;

		m_CommandPool = VkUtil::CreateCommandPool( m_Device, poolCreateInfo );

		for ( auto i = 0; i < m_CommandBuffers.size(); i++ )
			m_CommandBuffers[ i ] = VkUtil::CreateCommandBuffer( m_Device, m_CommandPool );

		// Create Swapchain...
		CreateFrameSizeDependantResources();

		// Create Sync Objects...
		VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_ImageAvailableSemaphores.resize( m_SwapchainImageViews.size() );
		m_RenderFinishedSemaphores.resize( m_SwapchainImageViews.size() );
		m_InFlightFences.resize( m_SwapchainImageViews.size() );

		for ( auto i = 0; i < m_SwapchainImageViews.size(); i++ ) {
			if ( vkCreateSemaphore( m_Device, &semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateSemaphore( m_Device, &semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateFence( m_Device, &fenceCreateInfo, nullptr, &m_InFlightFences[ i ] ) != VK_SUCCESS ) {
				printf( "Failed to create VkSemaphore objects or VkFence\n" );
				return;
			}
		}

		if ( vkCreateSemaphore( m_Device, &semaphoreCreateInfo, nullptr, &m_ComputeFinishedSemaphore ) != VK_SUCCESS ||
			vkCreateFence( m_Device, &fenceCreateInfo, nullptr, &m_ComputeInFlightFence ) != VK_SUCCESS ) {
			printf( "Failed to create compute VkSemaphore or VkFence\n" );
			return;
		}

		CreateBindlessTextureDescriptorSetLayout();
		CreateBindlessTextureDescriptors();

		auto psBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\TestPS.hlsl", ShaderType::PixelShader );
		auto vsBlob = m_ShaderCompiler.CompileShader( L"..\\Assets\\Shaders\\TestVS.hlsl", ShaderType::VertexShader );

		// This is temporary...
		m_DefaultPipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConstants ) } } )
			.SetDescriptorSets( { m_TexturePoolLayout } )
			.Build( m_Device );

		const auto depthFormat = VkUtil::PhysicalDeviceFindDepthFormat(m_PhysicalDevice);
		const auto imageFormat = VkUtil::SwapchainGetImageFormat(m_PhysicalDevice, m_Surface);

		m_DefaultPipeline = PipelineBuilder{}
			.SetShaderBlobs( { { psBlob, VK_SHADER_STAGE_FRAGMENT_BIT }, { vsBlob, VK_SHADER_STAGE_VERTEX_BIT } } )
			.SetColorAttachmentFormats( { imageFormat } )
			.SetDepthAttachmentFormat( depthFormat )
			.SetPipelineLayout( m_DefaultPipelineLayout )
			.Build( m_Device, m_Swapchain );

		auto texture = Image::LoadFromFile( m_Device, m_PhysicalDevice, m_CommandPool, m_GraphicsQueue, "..\\Assets\\Models\\default_texture.png" );

		m_SceneUniforms = new Buffer( m_Device, m_PhysicalDevice,
			Buffer::Desc{
				.m_Size = sizeof( SceneInfo ),
				.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.m_MemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				.m_SharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.m_AllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
			}
		);

		VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		textureView.image = *texture;
		textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		textureView.format = texture->GetFormat();
		textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		auto sampler = VkUtil::CreateSampler( m_Device );

		PushTexturesToTexturePool( { VkUtil::CreateImageView( m_Device, *texture, &textureView ) }, sampler );

		g_MainScene = std::make_unique<Scene>();

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

		UploadSceneMeshes( *g_MainScene );
		UploadMaterialTextures( *g_MainScene );
		UploadSceneMaterials(*g_MainScene);

		printf( "Finished Initializing\n" );
	}

	void Engine::Destroy() {
		vkDeviceWaitIdle( m_Device );

		// Destroy pipelines...
		vkDestroyPipelineLayout( m_Device, m_DefaultPipelineLayout, nullptr );
		vkDestroyPipeline( m_Device, m_DefaultPipeline, nullptr );

		vkFreeCommandBuffers( m_Device, m_CommandPool, uint32_t( m_CommandBuffers.size() ), m_CommandBuffers.data() );
		vkDestroyCommandPool( m_Device, m_CommandPool, nullptr );

		DestroyFrameSizeDependantResources();

		// Destroy sync objects...
		std::for_each( m_ImageAvailableSemaphores.begin(), m_ImageAvailableSemaphores.end(),
			[ & ]( auto&& semaphore )
		{
			vkDestroySemaphore( m_Device, semaphore, nullptr );
		}
		);

		std::for_each( m_RenderFinishedSemaphores.begin(), m_RenderFinishedSemaphores.end(),
			[ & ]( auto&& semaphore )
		{
			vkDestroySemaphore( m_Device, semaphore, nullptr );
		}
		);

		std::for_each( m_InFlightFences.begin(), m_InFlightFences.end(),
			[ & ]( auto&& fence )
		{
			vkDestroyFence( m_Device, fence, nullptr );
		}
		);

		vkDestroySemaphore( m_Device, m_ComputeFinishedSemaphore, nullptr );
		vkDestroyFence( m_Device, m_ComputeInFlightFence, nullptr );

		vkDestroyDevice( m_Device, nullptr );
		vkDestroySurfaceKHR( m_Instance, m_Surface, nullptr );
		vkDestroyInstance( m_Instance, nullptr );
	}

	void Engine::Tick() {
		BeginFrame();

		{
			VkImage currentImage = m_SwapchainImages[ m_CurrentImageIndex ];
			VkImageView currentView = m_SwapchainImageViews[ m_CurrentImageIndex ];

			VkImage depthImage = *m_DepthImages[0];
			VkImageView depthImageView = m_DepthImageViews[0];

			// Render...
			VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
			VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo( currentView, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
			VkRenderingAttachmentInfo depthAttachment = VkUtil::RenderPassGetDepthAttachmentInfo( depthImageView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );

			const auto& extents = m_SwapchainExtents;

			VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( extents, &colorAttachment, &depthAttachment );

			const auto& commandBuffer = GetCurrentCommandBuffer();

			VkUtil::CommandBufferImageBarrier(
				commandBuffer,
				depthImage,
				0,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
			);


			VkUtil::CommandBufferBeginRendering( commandBuffer, &renderingInfo );
			VkUtil::CommandBufferSetScissorAndViewport( commandBuffer, static_cast< float >( extents.width ), static_cast< float >( extents.height ) );

			vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline );

			auto defaultCamera = Camera::StationaryLookAtCamera( { 0.f, 150.f, 1.5f }, { 0.f, 150.f, 0.f }, { 0.f, 1.f, 0.f }, 90.f, extents.width / float( extents.height ), 0.1f, 1000.f );

			g_MainScene->SetMainCamera( defaultCamera );

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

	bool Engine::ShouldExit() {
		return glfwWindowShouldClose( m_GlfwWindow );
	}

	void Engine::BeginFrame() {
		vkWaitForFences( m_Device, 1, &m_InFlightFences[ m_CurrentFrame ], VK_TRUE, std::numeric_limits<uint64_t>::max() );
		vkResetFences( m_Device, 1, &m_InFlightFences[ m_CurrentFrame ] );

		const auto result = vkAcquireNextImageKHR( m_Device, m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[ m_CurrentFrame ], VK_NULL_HANDLE, &m_CurrentImageIndex );
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

		VkResult result = vkQueueSubmit( m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[ m_CurrentFrame ] );
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

		result = vkQueuePresentKHR( m_PresentQueue, &presentInfo );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
			DestroyFrameSizeDependantResources();
			CreateFrameSizeDependantResources();
		} else if ( result != VK_SUCCESS ) {
			printf( "Failed to present swap chain image!" );
		}

		m_CurrentFrame = ( m_CurrentFrame + 1 ) % MaxFramesInFlight;
	}

	void Engine::CreateFrameSizeDependantResources() {
		m_Swapchain = VkUtil::CreateSwapchain( m_Device, m_PhysicalDevice, m_Surface, m_WindowHandle, m_SwapchainExtents );

		const auto imageFormat = VkUtil::SwapchainGetImageFormat( m_PhysicalDevice, m_Surface );
		VkUtil::CreateSwapchainImages( m_Device, m_Swapchain, imageFormat, m_SwapchainImages, m_SwapchainImageViews );

		const auto depthFormat = VkUtil::PhysicalDeviceFindDepthFormat(m_PhysicalDevice);
		for(auto i = 0; i < m_DepthImages.size(); i++) {
			m_DepthImages[i] = new Image(
				m_Device,
				m_PhysicalDevice,
				Image::Desc{ 
					.m_Width = m_SwapchainExtents.width,
					.m_Height = m_SwapchainExtents.height,
					.m_Levels = 1,
					.m_Format = depthFormat,
					.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
					.m_Samples = VK_SAMPLE_COUNT_1_BIT,
					.m_Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
					.m_Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
				}
			);

			VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			textureView.image = *m_DepthImages[ i ];
			textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			textureView.format = depthFormat;
			textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
			textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

			m_DepthImageViews[i] = VkUtil::CreateImageView(m_Device, *m_DepthImages[i], &textureView);
		}
	}

	void Engine::DestroyFrameSizeDependantResources() {
		vkDeviceWaitIdle( m_Device );

		std::for_each( m_SwapchainImageViews.begin(), m_SwapchainImageViews.end(),
			[ & ]( const auto imageView )
		{
			vkDestroyImageView( m_Device, imageView, nullptr );
		});

		std::for_each(m_DepthImages.begin(), m_DepthImages.end(),
			[& ](const auto image) {
			
			delete image;
		});

		std::for_each( m_DepthImageViews.begin(), m_DepthImageViews.end(),
			[ & ]( const auto imageView )
		{
			vkDestroyImageView( m_Device, imageView, nullptr );
		} );

		m_SwapchainImages.clear();
		m_SwapchainImageViews.clear();

		vkDestroySwapchainKHR( m_Device, m_Swapchain, nullptr );
	}

	void Engine::CreateBindlessTextureDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024u, VK_SHADER_STAGE_ALL };
		VkDescriptorBindingFlags descriptorBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		descriptorSetLayoutBindingFlags.pBindingFlags = &descriptorBindingFlags;
		descriptorSetLayoutBindingFlags.bindingCount = 1;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlags;
		descriptorSetLayoutCreateInfo.bindingCount = 1;
		descriptorSetLayoutCreateInfo.pBindings = &texturesDescriptorSetLayoutBinding;

		vkCreateDescriptorSetLayout( m_Device, &descriptorSetLayoutCreateInfo, nullptr, &m_TexturePoolLayout );
	}

	void Engine::CreateBindlessTextureDescriptors() {
		VkDescriptorPoolSize descriptorPoolSizes[ ] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.pNext = NULL;
		descriptorPoolCreateInfo.flags = 0;
		descriptorPoolCreateInfo.maxSets = 1;
		descriptorPoolCreateInfo.poolSizeCount = _countof( descriptorPoolSizes );
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

		vkCreateDescriptorPool( m_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool );

		VkDescriptorSetAllocateInfo setAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		setAllocateInfo.descriptorPool = m_DescriptorPool;
		setAllocateInfo.pSetLayouts = &m_TexturePoolLayout;
		setAllocateInfo.descriptorSetCount = 1;
		vkAllocateDescriptorSets( m_Device, &setAllocateInfo, &m_TexturePool );
	}

	void Engine::PushTexturesToTexturePool( const std::vector<VkImageView>& textures, VkSampler sampler ) {
		std::vector<VkDescriptorImageInfo> imageInfos( textures.size() );
		for ( auto i = 0; i < imageInfos.size(); i++ ) {
			auto& info = imageInfos[ i ];
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = textures[ i ];
			info.sampler = sampler;
		}

		VkWriteDescriptorSet writeDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDescriptorSet.dstSet = m_TexturePool;
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = static_cast< uint32_t >( textures.size() );
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pImageInfo = imageInfos.data();

		vkUpdateDescriptorSets( m_Device, 1, &writeDescriptorSet, 0, nullptr );
	}

	void Engine::UploadSceneMaterials( Scene& scene ) { 
		std::vector<GPUMaterial> materials{};

		auto& registry = scene.GetRegistry();
		for ( auto ent : registry.view<Material>() ) {
			Material& mat = registry.get<Material>( ent );

			GPUMaterial gpuMat = {};
			gpuMat.m_Index = mat.m_Index;
			gpuMat.m_MetallicFactor = mat.m_MetallicFactor;
			gpuMat.m_RoughnessFactor = mat.m_RoughnessFactor;
			gpuMat.m_AlphaMode = mat.m_AlphaMode;
			gpuMat.m_AlphaCutoff = mat.m_AlphaCutoff;
			gpuMat.m_AlbedoTexture = mat.m_AlbedoTexture;
			gpuMat.m_NormalsTexture = mat.m_NormalsTexture;
			gpuMat.m_MetalRoughnessTexture = mat.m_MetalRoughnessTexture;
			gpuMat.m_EmissiveTexture = mat.m_EmissiveTexture;
			gpuMat.m_Albedo = mat.m_Albedo;
			gpuMat.m_Emissive = mat.m_Emissive;

			materials.push_back( gpuMat );
		}
	
		std::sort( materials.begin(), materials.end(), [&](const auto& a, const auto& b) -> bool {
		 	return a.m_Index < b.m_Index;
		});

		if(!m_SceneMaterials )
			m_SceneMaterials = new Buffer(m_Device, m_PhysicalDevice, 
				Buffer::Desc {
					.m_Size = materials.size() * sizeof(GPUMaterial),
					.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					.m_MemoryFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
					.m_SharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.m_AllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
				});

		m_SceneMaterials->Patch(materials.data(), materials.size() * sizeof(GPUMaterial));
	}

	void Engine::UploadMaterialTextures( Scene& scene ) {
		auto& registry = scene.GetRegistry();

		std::vector<VkImageView> images{};

		int textureIndex = 0;

		for(auto ent : registry.view<Material>()) {
			Material& mat = registry.get<Material>(ent);

			if(!mat.m_AlbedoTexturePath.empty()) {
				Image* img = Image::LoadFromFile(m_Device, m_PhysicalDevice, m_CommandPool, m_GraphicsQueue, mat.m_AlbedoTexturePath);
			
				VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				textureView.image = *img;
				textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
				textureView.format = img->GetFormat();
				textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
				textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				images.push_back( VkUtil::CreateImageView( m_Device, *img, &textureView ) );

				mat.m_AlbedoTexture = textureIndex++;
			}
		}

		auto sampler = VkUtil::CreateSampler(m_Device, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, 0.f);
	
		PushTexturesToTexturePool( images, sampler );
	}

	void Engine::RenderScene( Scene& scene ) {
		VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();

		auto& registry = scene.GetRegistry();
		
		const auto& camera = scene.GetMainCamera();

		SceneInfo sceneInfo = {};
		sceneInfo.m_CameraViewProjectionMatrix = camera.GetViewProjectionMatrix();
		sceneInfo.m_SunColor = { 1.f, 1.f, 1.f, 1.f };
		sceneInfo.m_SunDirection = glm::normalize( glm::vec4{ -1.5f, 0.f, -.075f, 1.f } );

		m_SceneUniforms->Patch( &sceneInfo, sizeof( sceneInfo ) );

		for ( entt::entity entity : registry.view<Mesh>() ) {
			const Mesh& mesh = registry.get<Mesh>( entity );

			// TODO: Make transforms not optional... They should be the first component on every entity.
			glm::mat4 modelMatrix( 1.f );

			if ( Transform* transform = registry.try_get<Transform>( entity ) ) {
				modelMatrix = transform->GetWorldTransform();
			} else {
				modelMatrix = glm::rotate( modelMatrix, float( glfwGetTime() ), { 0.f, 1.f, 0.f } );
			}

			uint32_t materialIndex = mesh.m_Material;

			PushConstants pushConstants = {
				m_SceneUniforms->GetDeviceAddress(),
				m_SceneMaterials->GetDeviceAddress(),
				mesh.m_VertexBuffer->GetDeviceAddress(),
				// modelMatrix,
				materialIndex
			};

			vkCmdPushConstants( commandBuffer, m_DefaultPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pushConstants ), &pushConstants );
			vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipelineLayout, 0, 1, &m_TexturePool, 0, nullptr );
		
			if(!mesh.m_Indices.empty()) {
				vkCmdBindIndexBuffer( commandBuffer, *(mesh.m_IndexBuffer), 0, VK_INDEX_TYPE_UINT32 );
				vkCmdDrawIndexed( commandBuffer, uint32_t( mesh.m_Indices.size() ), 1, 0, 0, 0);
			} else {
				vkCmdDraw( commandBuffer, uint32_t( mesh.m_Vertices.size() ), 1, 0, 0 );
			}
		}
	}

	void Engine::UploadSceneMeshes( Scene& scene ) {
		auto& registry = scene.GetRegistry();

		for ( entt::entity meshEntities : registry.view<Mesh>() ) {
			Mesh& mesh = registry.get<Mesh>( meshEntities );

			if ( !mesh.m_VertexBuffer ) {
				mesh.m_VertexBuffer = new Buffer( m_Device, m_PhysicalDevice,
					Buffer::Desc{
						.m_Size = mesh.m_Vertices.size() * sizeof( MeshVertexData ),
						.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.m_MemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						.m_SharingMode = VK_SHARING_MODE_EXCLUSIVE,
						.m_AllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
					}
				);
			}

			if(!mesh.m_IndexBuffer && !mesh.m_Indices.empty()) {
				mesh.m_IndexBuffer = new Buffer( m_Device, m_PhysicalDevice,
					Buffer::Desc{
						.m_Size = mesh.m_Indices.size() * sizeof( uint32_t ),
						.m_Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.m_MemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						.m_SharingMode = VK_SHARING_MODE_EXCLUSIVE,
						.m_AllocateFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
					}
				);
			}

			if(!mesh.m_Indices.empty())
				mesh.m_IndexBuffer->Patch( mesh.m_Indices.data(), mesh.m_Indices.size() * sizeof( uint32_t ) );

			mesh.m_VertexBuffer->Patch( mesh.m_Vertices.data(), mesh.m_Vertices.size() * sizeof( MeshVertexData ) );
		}
	}

	void Engine::UpdateSceneMeshData( Scene& scene ) {

	}
}