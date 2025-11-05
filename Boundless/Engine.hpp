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
		IDxcBlob* SkinningShader;
	};

	extern EngineShaders g_EngineShaders;

	struct FrameConstants {
		glm::mat4  m_CameraViewProjectionMatrix;
		glm::mat4  m_CameraInvViewProjectionMatrix;
		glm::vec4  m_CameraPosition;
		glm::vec4  m_SunDirection;
		glm::vec4  m_SunColor;
		glm::ivec4 m_IblTextures;
	};

	struct FrameData {
		CommandBuffer   m_CommandBuffer;
		vk::CommandPool m_CommandPool;
		vk::Semaphore   m_ImageAvailableSemaphore;
		vk::Fence	    m_InFlightFence;
	};

	class Engine {
	public:
		explicit Engine(GLFWwindow* window);
		~Engine();

		void Tick( float deltaTime );

		bool ShouldExit();

		constexpr static const int MaxFramesInFlight = 2;

		FrameData& GetCurrentFrame() { return m_FrameData[ m_CurrentFrame % MaxFramesInFlight ]; }
		CommandBuffer& GetCommandBuffer() { return GetCurrentFrame().m_CommandBuffer; }
	private:
		void Update( float deltaTime );
		void Render( float deltaTime );

		void OnResize();
		void RecompilePasses();

		void RenderFinalPass( CommandBuffer& commandBuffer, ImageHandle texture );
		void DestroySwapchain();
	private:
		GLFWwindow*									   m_GlfwWindow;
		HWND										   m_WindowHandle = 0;
		std::unique_ptr<Device>						   m_Device;
		vk::Extent2D								   m_SwapchainExtents;
		std::vector<vk::ImageView>					   m_SwapchainImageViews;
		std::vector<vk::Image>						   m_SwapchainImages;
		std::vector<vk::Semaphore>					   m_SwapchainRenderFinishedSemaphores;
		vk::Format									   m_SwapchainImageFormat;
		vk::SwapchainKHR							   m_Swapchain = VK_NULL_HANDLE;
		std::array<FrameData, MaxFramesInFlight>	   m_FrameData;
		bool										   m_ResizeRequested = false;
		//vk::Semaphore								   m_ComputeFinishedSemaphore = VK_NULL_HANDLE;
		//vk::Fence									   m_ComputeInFlightFence = VK_NULL_HANDLE;
		uint32_t									   m_CurrentImageIndex = 0;
		uint32_t									   m_CurrentFrame = 0;
		ShaderCompiler								   m_ShaderCompiler;
		FrameConstants								   m_FrameConstants = {};
		BufferHandle								   m_FrameConstantsBuffer = BufferHandle::Invalid;
		vk::Pipeline								   m_FullscreenPipeline = VK_NULL_HANDLE;
		std::unique_ptr<GBufferPass>				   m_GBuffer;
		std::unique_ptr<GBufferDebugPass>			   m_GBufferDebug;
		std::unique_ptr<LightingPass>				   m_Lighting;
		std::unique_ptr<CompositePass>				   m_Composite;
		std::vector<BaseRenderPass*>				   m_RenderPasses;
		Scene										   m_Scene; // This should maybe be moved somewhere else.

		// TODO: move/remove these.
		void GenerateBrdfLut();
		ImageHandle m_BrdfLut	  = ImageHandle::Invalid;
		ImageHandle m_DiffuseIbl  = ImageHandle::Invalid;
		ImageHandle m_SpecularIbl = ImageHandle::Invalid;
	};
}