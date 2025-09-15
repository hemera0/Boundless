#pragma once
#include "VkUtil.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "Shaders.hpp"
#include "Pipelines.hpp"
#include "Scene.hpp"

#include <entt/entt.hpp>

namespace Boundless {
	class Engine {
	public:
		static Engine* s_Instance;

		Engine() {
			s_Instance = this;
		}

		static Engine* GetInstance() {
			return s_Instance;
		}

		void Create();
		void Destroy();

		void Tick();

		bool ShouldExit();
	private:
		constexpr static const int MaxFramesInFlight = 2;

		void BeginFrame();
		void EndFrame();

		void CreateFrameSizeDependantResources();
		void DestroyFrameSizeDependantResources();

		HWND m_WindowHandle{};
		GLFWwindow* m_GlfwWindow = nullptr;

		VkInstance m_Instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;

		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VkQueue m_ComputeQueue = VK_NULL_HANDLE;

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
		std::array<VkCommandBuffer, MaxFramesInFlight> m_CommandBuffers{};

		VkExtent2D m_SwapchainExtents{};
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_SwapchainImages{};
		std::vector<VkImageView> m_SwapchainImageViews{};

		std::array<VkImage, MaxFramesInFlight> m_DepthImages{};
		std::array<VkImageView, MaxFramesInFlight> m_DepthImageViews{};

		// Sync Objects...
		std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
		std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
		std::vector<VkFence> m_InFlightFences{};

		VkSemaphore m_ComputeFinishedSemaphore = VK_NULL_HANDLE;
		VkFence m_ComputeInFlightFence = VK_NULL_HANDLE;

		uint32_t m_CurrentImageIndex{};
		uint32_t m_CurrentFrame{};

		VkCommandBuffer GetCurrentCommandBuffer() {
			return m_CommandBuffers[m_CurrentFrame];
		}

		// Shader Compilation...
		std::unique_ptr<ShaderCompiler> m_ShaderCompiler = {};

		// Default Pipeline...
		VkPipelineLayout m_DefaultPipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_DefaultPipeline = VK_NULL_HANDLE;

		Buffer* m_VertexPool = nullptr;

		// Descriptors...
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_TexturePoolLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_TexturePool = VK_NULL_HANDLE;

		void CreateBindlessTextureDescriptorSetLayout();
		void CreateBindlessTextureDescriptors();
		void PushTexturesToTexturePool(const std::vector<VkImageView>& textures, VkSampler sampler);

		Buffer* GrowBuffer(const VkDevice& device, const VkPhysicalDevice& physicalDevice, Buffer* buffer, const Boundless::BufferDesc& bufferDesc);

		void RenderScene(Scene& scene);
	};
}