#pragma once
#include "VkUtil.hpp"
#include "Device.hpp"
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

		void Tick( float dt );

		bool ShouldExit();
	private:
		void Update(float dt);
		void Render();

		constexpr static const int MaxFramesInFlight = 2;

		void BeginFrame();
		void EndFrame();

		void CreateFrameSizeDependantResources();
		void DestroyFrameSizeDependantResources();

		HWND m_WindowHandle{};
		GLFWwindow* m_GlfwWindow = nullptr;

		// Vulkan Data.
		std::unique_ptr<Device> m_Device{};
		std::array<VkCommandBuffer, MaxFramesInFlight> m_CommandBuffers{};

		VkExtent2D m_SwapchainExtents{};
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_SwapchainImages{};
		std::vector<VkImageView> m_SwapchainImageViews{};

		std::array<Image, MaxFramesInFlight> m_DepthImages{};
		std::array<VkImageView, MaxFramesInFlight> m_DepthImageViews{};

		// Vulkan Sync Objects...
		std::vector<VkSemaphore> m_ImageAvailableSemaphores{};
		std::vector<VkSemaphore> m_RenderFinishedSemaphores{};
		std::vector<VkFence> m_InFlightFences{};

		VkSemaphore m_ComputeFinishedSemaphore = VK_NULL_HANDLE;
		VkFence m_ComputeInFlightFence = VK_NULL_HANDLE;

		uint32_t m_CurrentImageIndex{};
		uint32_t m_CurrentFrame{};

		VkCommandBuffer GetCurrentCommandBuffer() { return m_CommandBuffers[m_CurrentFrame]; }

		// Shader Compilation...
		ShaderCompiler m_ShaderCompiler = {};

		// TODO: Move...
		// Default Pipeline...
		VkPipelineLayout m_DefaultPipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_DefaultPipeline = VK_NULL_HANDLE;
		
		void RenderScene( Scene& scene );
	};
}