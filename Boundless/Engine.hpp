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

	struct FrameConstants {
		glm::mat4  m_CameraViewProjectionMatrix;
		glm::mat4  m_CameraInvViewProjectionMatrix;
		glm::vec4  m_CameraPosition;
		glm::vec4  m_SunDirection;
		glm::vec4  m_SunColor;
		glm::ivec4 m_IblTextures;
	};

	class Engine {
	public:
		Engine();
		~Engine();

		void Tick( float deltaTime );

		bool ShouldExit();

		CommandBuffer& GetCommandBuffer() { return m_CommandBuffers[ m_CurrentFrame ]; }
	private:
		void Update( float deltaTime );
		void Render( float deltaTime );

		void OnResize( );
		void RecompilePasses();

		void RenderFinalPass( CommandBuffer& commandBuffer, ImageHandle texture );
		void DestroySwapchain();
	public:
		constexpr static const int MaxFramesInFlight = 2;
	private:
		GLFWwindow*									   m_GlfwWindow;
		HWND										   m_WindowHandle;
		std::unique_ptr<Device>						   m_Device;
		std::array<CommandBuffer, MaxFramesInFlight>   m_CommandBuffers;
		VkExtent2D									   m_SwapchainExtents;
		VkSwapchainKHR								   m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage>						   m_SwapchainImages;
		std::vector<VkImageView>					   m_SwapchainImageViews;
		std::vector<VkSemaphore>					   m_ImageAvailableSemaphores;
		std::vector<VkSemaphore>					   m_RenderFinishedSemaphores;
		std::vector<VkFence>						   m_InFlightFences;
		VkSemaphore									   m_ComputeFinishedSemaphore = VK_NULL_HANDLE;
		VkFence										   m_ComputeInFlightFence = VK_NULL_HANDLE;
		uint32_t									   m_CurrentImageIndex = 0;
		uint32_t									   m_CurrentFrame = 0;
		ShaderCompiler								   m_ShaderCompiler;
		FrameConstants								   m_FrameConstants = {};
		VkPipeline									   m_FullscreenPipeline = VK_NULL_HANDLE;
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