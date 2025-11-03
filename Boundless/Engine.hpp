#pragma once
#include "VkUtil.hpp"
#include "Device.hpp"
#include "Shaders.hpp"
#include "Pipelines.hpp"
#include "Scene.hpp"

#include "RenderPasses.hpp"

namespace Boundless {
	// TODO: Shader classes for organization...
	struct EngineShaders { 
		IDxcBlob* GBufferPixelShader;
		IDxcBlob* GBufferVertexShader;
		IDxcBlob* GBufferDebugPixelShader;
		IDxcBlob* CompositePixelShader;
		IDxcBlob* LightingPixelShader;
		IDxcBlob* FullscreenTriVertexShader;
		IDxcBlob* FullscreenBlitPixelShader;
		IDxcBlob* BrdfGenPixelShader;
	};

	extern EngineShaders g_EngineShaders;

	class Engine {
	public:
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

		VkExtent2D				 m_SwapchainExtents;
		VkSwapchainKHR			 m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage>	 m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;

		// TODO: Remove these they will be in the RenderGraph.
		// ImageHandle m_HDRFrameBuffer      = ImageHandle::Invalid;
		// ImageHandle m_HDRFrameBufferView  = ImageHandle::Invalid;
		// ImageHandle m_HdrHandle			  = ImageHandle::Invalid;
		// ImageHandle m_MSAAFrameBuffer	  = ImageHandle::Invalid;
		// ImageHandle m_MSAAFrameBufferView = ImageHandle::Invalid;

		// std::array<ImageHandle, MaxFramesInFlight> m_DepthImages;
		// std::array<ImageHandle, MaxFramesInFlight> m_DepthImageViews;

		// Vulkan Sync Objects...
		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;

		VkSemaphore m_ComputeFinishedSemaphore = VK_NULL_HANDLE;
		VkFence m_ComputeInFlightFence = VK_NULL_HANDLE;
		uint32_t m_CurrentImageIndex{};
		uint32_t m_CurrentFrame{};

		VkCommandBuffer GetCurrentCommandBuffer() { return m_CommandBuffers[m_CurrentFrame]; }

		// Shader Compilation...
		ShaderCompiler m_ShaderCompiler = {};

		// TODO: Move...
		// VkPipelineLayout m_DefaultPipelineLayout	= VK_NULL_HANDLE;
		// VkPipeline m_DefaultPipeline				= VK_NULL_HANDLE;
		VkPipelineLayout m_FullscreenPipelineLayout = VK_NULL_HANDLE;
		VkPipeline m_FullscreenPipeline				= VK_NULL_HANDLE;

		void GenerateBrdfLut();

		// Render Passes.
		struct {
			std::unique_ptr<GBufferPass>	  m_GBuffer;
			std::unique_ptr<GBufferDebugPass> m_GBufferDebug;
			std::unique_ptr<LightingPass>	  m_Lighting;
			std::unique_ptr<CompositePass>	  m_Composite;
			std::vector<BaseRenderPass*>	  m_RenderPasses;
		};

		Scene m_Scene;

		ImageHandle m_BrdfLut	  = ImageHandle::Invalid;
		ImageHandle m_DiffuseIbl  = ImageHandle::Invalid;
		ImageHandle m_SpecularIbl = ImageHandle::Invalid;
	};
}