#define VOLK_IMPLEMENTATION
#include "Engine.hpp"

#include "OBJImporter.hpp"

Boundless::Engine* Boundless::Engine::s_Instance = nullptr;

struct alignas(16) Color {
	float R, G, B, A;
};

struct PushConstants {
	glm::mat4 MVP;
	VkDeviceAddress m_VertexBufferAddress;
};

std::unique_ptr<Boundless::Scene> g_MainScene{};

void Boundless::Engine::Create() {
	volkInitialize();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_GlfwWindow = glfwCreateWindow(1280, 720, "Boundless", nullptr, nullptr);
	if (!m_GlfwWindow) {
		printf("Failed to create engine window\n");
		return;
	}

	// glfwSetFramebufferSizeCallback(m_EngineGlfwWindow, &FramebufferResizeCallback);
	// glfwSetCursorPosCallback(m_EngineGlfwWindow, &MouseCallback);
	// glfwSetKeyCallback(m_EngineGlfwWindow, &KeyCallback);
	// glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowUserPointer(m_GlfwWindow, this);

	m_WindowHandle = glfwGetWin32Window(m_GlfwWindow);

	uint32_t extensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	const std::vector<const char*> extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
	};

	m_Instance = VkUtil::CreateInstance(glfwExtensions, extensionCount);
	if (m_Instance == VK_NULL_HANDLE)
		return;

	volkLoadInstance(m_Instance);

	m_PhysicalDevice = VkUtil::GetPhysicalDevice(m_Instance, extensions);
	m_Surface = VkUtil::CreateSurfaceForWindow(m_Instance, m_WindowHandle);
	m_Device = VkUtil::CreateLogicalDevice(m_Instance, m_PhysicalDevice, m_Surface, extensions);

	const auto& queueFamilyIndices = VkUtil::FindQueueFamilyIndices(m_Surface, m_PhysicalDevice);
	m_GraphicsQueue = VkUtil::DeviceGetQueueByIndex(m_Device, queueFamilyIndices.m_GraphicsFamilyIndex);
	m_PresentQueue = VkUtil::DeviceGetQueueByIndex(m_Device, queueFamilyIndices.m_PresentFamilyIndex);
	m_ComputeQueue = VkUtil::DeviceGetQueueByIndex(m_Device, queueFamilyIndices.m_ComputeFamilyIndex);

	VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolCreateInfo.queueFamilyIndex = queueFamilyIndices.m_GraphicsFamilyIndex;

	m_CommandPool = VkUtil::CreateCommandPool(m_Device, poolCreateInfo);

	for (auto i = 0; i < m_CommandBuffers.size(); i++)
		m_CommandBuffers[i] = VkUtil::CreateCommandBuffer(m_Device, m_CommandPool);

	// Create Swapchain...
	CreateFrameSizeDependantResources();

	// Create Sync Objects...
	VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	m_ImageAvailableSemaphores.resize(m_SwapchainImageViews.size());
	m_RenderFinishedSemaphores.resize(m_SwapchainImageViews.size());
	m_InFlightFences.resize(m_SwapchainImageViews.size());

	for (auto i = 0; i < m_SwapchainImageViews.size(); i++) {
		if (vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
		{
			printf("Failed to create VkSemaphore objects or VkFence\n");
			return;
		}
	}

	if (vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_ComputeFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_ComputeInFlightFence) != VK_SUCCESS)
	{
		printf("Failed to create compute VkSemaphore or VkFence\n");
		return;
	}

	m_ShaderCompiler = std::make_unique<ShaderCompiler>();

	auto psBlob = m_ShaderCompiler->CompileShader(L"..\\Assets\\Shaders\\TestPS.hlsl", ShaderType::PixelShader);
	auto vsBlob = m_ShaderCompiler->CompileShader(L"..\\Assets\\Shaders\\TestVS.hlsl", ShaderType::VertexShader);

	CreateBindlessTextureDescriptorSetLayout();
	CreateBindlessTextureDescriptors();

	m_DefaultPipelineLayout = PipelineLayoutBuilder()
		.SetPushConstants({ VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants) } })
		.SetDescriptorSets({ m_TexturePoolLayout })
		.Build(m_Device);

	m_DefaultPipeline = PipelineBuilder{}
		.SetShaderBlobs({ { psBlob, VK_SHADER_STAGE_FRAGMENT_BIT }, { vsBlob, VK_SHADER_STAGE_VERTEX_BIT } })
		.SetPipelineLayout(m_DefaultPipelineLayout)
		.Build(m_Device, m_Swapchain);

	auto texture = Image::LoadFromFile(m_Device, m_PhysicalDevice, m_CommandPool, m_GraphicsQueue, "..\\Assets\\Models\\default_texture.png");

	m_VertexPool = new Buffer(m_Device, m_PhysicalDevice, 10000, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

	VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	textureView.image = *texture;
	textureView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	textureView.format = texture->GetFormat();
	textureView.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	textureView.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	textureView.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	textureView.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	textureView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureView.subresourceRange.baseMipLevel = 0;
	textureView.subresourceRange.levelCount = 1;
	textureView.subresourceRange.baseArrayLayer = 0;
	textureView.subresourceRange.layerCount = 1;

	auto sampler = VkUtil::CreateSampler(m_Device);

	PushTexturesToTexturePool({ VkUtil::CreateImageView(m_Device, *texture, &textureView) }, sampler);

	g_MainScene = std::make_unique<Scene>();

	OBJImporter importer(*g_MainScene);
	if (!importer.LoadFromFile("..\\Assets\\Models\\suzanne.obj")) {
		printf("Failed to load obj file\n");
		return;
	}

	printf("Finished Initializing\n");
}

void Boundless::Engine::Destroy() {
	vkDeviceWaitIdle(m_Device);
	vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
}

void Boundless::Engine::Tick()
{
	BeginFrame();

	{
		VkImage currentImage = m_SwapchainImages[m_CurrentImageIndex];
		VkImageView currentView = m_SwapchainImageViews[m_CurrentImageIndex];

		// Render...
		VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
		VkRenderingAttachmentInfo colorAttachment = VkUtil::RenderPassGetColorAttachmentInfo(currentView, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);

		const auto& extents = m_SwapchainExtents;

		VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo(extents, &colorAttachment, nullptr);

		const auto& commandBuffer = GetCurrentCommandBuffer();

		VkUtil::CommandBufferBeginRendering(commandBuffer, &renderingInfo);
		VkUtil::CommandBufferSetScissorAndViewport(commandBuffer, static_cast<float>(extents.width), static_cast<float>(extents.height));
		
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipeline);

		auto defaultCamera = Camera::StationaryLookAtCamera({ 0.f, 2.f, 1.5f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, 90.f, extents.width / float(extents.height), 0.f, 100.f);

		g_MainScene->SetMainCamera(defaultCamera);

		RenderScene(*g_MainScene);
		
		VkUtil::CommandBufferEndRendering(commandBuffer);

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

bool Boundless::Engine::ShouldExit()
{
	return glfwWindowShouldClose(m_GlfwWindow);
}

void Boundless::Engine::BeginFrame()
{
	vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

	const auto result = vkAcquireNextImageKHR(m_Device, m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		DestroyFrameSizeDependantResources();
		CreateFrameSizeDependantResources();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		printf("Failed to acquire swap chain image\n");
	}

	vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
	VkUtil::CommandBufferBegin(m_CommandBuffers[m_CurrentFrame]);
}

void Boundless::Engine::EndFrame()
{
	VkUtil::CommandBufferEnd(m_CommandBuffers[m_CurrentFrame]);

	VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

	VkCommandBuffer commandBuffers[] = { m_CommandBuffers[m_CurrentFrame] };

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = _countof(waitSemaphores);
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = _countof(commandBuffers);
	submitInfo.pCommandBuffers = commandBuffers;

	VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentImageIndex] };
	submitInfo.signalSemaphoreCount = _countof(signalSemaphores);
	submitInfo.pSignalSemaphores = signalSemaphores;

	VkResult result = vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]);
	if (result != VK_SUCCESS)
	{
		printf("Failed to submit draw command buffer: %d\n", result);
	}

	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = _countof(signalSemaphores);
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { m_Swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &m_CurrentImageIndex;

	result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		DestroyFrameSizeDependantResources();
		CreateFrameSizeDependantResources();
	}
	else if (result != VK_SUCCESS)
	{
		printf("Failed to present swap chain image!");
	}

	m_CurrentFrame = (m_CurrentFrame + 1) % MaxFramesInFlight;
}

void Boundless::Engine::CreateFrameSizeDependantResources()
{
	m_Swapchain = VkUtil::CreateSwapchain(m_Device, m_PhysicalDevice, m_Surface, m_WindowHandle, m_SwapchainExtents);

	const auto imageFormat = VkUtil::SwapchainGetImageFormat(m_PhysicalDevice, m_Surface);
	VkUtil::CreateSwapchainImages(m_Device, m_Swapchain, imageFormat, m_SwapchainImages, m_SwapchainImageViews);
}

void Boundless::Engine::DestroyFrameSizeDependantResources()
{
	vkDeviceWaitIdle(m_Device);

	std::for_each(m_SwapchainImageViews.begin(), m_SwapchainImageViews.end(),
		[&](const auto imageView) {
			vkDestroyImageView(m_Device, imageView, nullptr);
		}
	);

	m_SwapchainImageViews.clear();

	vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

void Boundless::Engine::CreateBindlessTextureDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024u, VK_SHADER_STAGE_ALL };
	VkDescriptorBindingFlags descriptorBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
	descriptorSetLayoutBindingFlags.pBindingFlags = &descriptorBindingFlags;
	descriptorSetLayoutBindingFlags.bindingCount = 1;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlags;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &texturesDescriptorSetLayoutBinding;

	vkCreateDescriptorSetLayout(m_Device, &descriptorSetLayoutCreateInfo, nullptr, &m_TexturePoolLayout);
}

void Boundless::Engine::CreateBindlessTextureDescriptors()
{
	CreateBindlessTextureDescriptorSetLayout();

	VkDescriptorPoolSize descriptorPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 }
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = NULL;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = _countof(descriptorPoolSizes);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

	vkCreateDescriptorPool(m_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool);

	//uint32_t descriptor_counts = static_cast<uint32_t>(textures.size());

	//VkDescriptorSetVariableDescriptorCountAllocateInfoEXT descriptorSetVariableDescriptorCountAllocateInfoEXT = {};
	//descriptorSetVariableDescriptorCountAllocateInfoEXT.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	//descriptorSetVariableDescriptorCountAllocateInfoEXT.descriptorSetCount = 1;
	//descriptorSetVariableDescriptorCountAllocateInfoEXT.pDescriptorCounts = &descriptor_counts;

	VkDescriptorSetAllocateInfo set_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	set_info.descriptorPool = m_DescriptorPool;
	set_info.pSetLayouts = &m_TexturePoolLayout;
	set_info.descriptorSetCount = 1;
	vkAllocateDescriptorSets(m_Device, &set_info, &m_TexturePool);
}

void Boundless::Engine::PushTexturesToTexturePool(const std::vector<VkImageView>& textures, VkSampler sampler) {
	std::vector<VkDescriptorImageInfo> imageInfos(textures.size());
	for (auto i = 0; i < imageInfos.size(); i++) {
		auto& info = imageInfos[i];
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		info.imageView = textures[i];
		info.sampler = sampler;
	}

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = m_TexturePool;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.descriptorCount = static_cast<uint32_t>(textures.size());
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_Device, 1, &writeDescriptorSet, 0, nullptr);
}

Boundless::Buffer* Boundless::Engine::GrowBuffer(const VkDevice& device, const VkPhysicalDevice& physicalDevice, Buffer* buffer, const Boundless::BufferDesc& bufferDesc)
{
	if (buffer)
	{
		if (bufferDesc.GetSize() <= buffer->GetSize())
			return buffer;

		delete buffer;
		buffer = nullptr;
	}

	return new Boundless::Buffer(device, physicalDevice, bufferDesc);
}

void Boundless::Engine::RenderScene(Scene& scene)
{
	VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();

	auto& registry = scene.GetRegistry();

	int idx = 1;

	for (entt::entity meshEntities : registry.view<Mesh>()) {
		Mesh& mesh = registry.get<Mesh>(meshEntities);

		mesh.m_VertexBuffer = GrowBuffer(m_Device, m_PhysicalDevice, mesh.m_VertexBuffer,
			BufferDesc{
				 mesh.m_Vertices.size() * sizeof(MeshVertexData),
				 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				 VK_SHARING_MODE_EXCLUSIVE,
				 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
			}
		);

		m_VertexPool->Patch(mesh.m_Vertices.data(), mesh.m_Vertices.size() * sizeof(MeshVertexData));
	}

	for (entt::entity meshEntities : registry.view<Mesh>()) {
		const Mesh& mesh = registry.get<Mesh>(meshEntities);

		// TODO: Replace with transforms...
		glm::mat4 modelTranslation(1.f);

		// modelTranslation = glm::translate(modelTranslation, { 0.f, idx * 0.75f, 0.f });
		modelTranslation = glm::rotate(modelTranslation, float(glfwGetTime()), { 0.f, 1.f, 0.f });

		const auto& camera = scene.GetMainCamera();
		const auto& cameraMatrix = camera.GetViewProjectionMatrix();

		struct {
			glm::mat4 m_ModelViewProjection;
			VkDeviceAddress m_VertexBufferAddress;
		} PushConstants = {
				cameraMatrix * modelTranslation,
				mesh.m_VertexBuffer->GetDeviceAddress()
		};

		vkCmdPushConstants(commandBuffer, m_DefaultPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &PushConstants);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DefaultPipelineLayout, 0, 1, &m_TexturePool, 0, nullptr);
		vkCmdDraw(commandBuffer, uint32_t(mesh.m_Vertices.size()), 1, 0, 0);

		idx++;
	}
}