#pragma once
#include "Pch.hpp"

// Generic Vulkan boilerplate code.
enum class ResourceHandle : uint32_t { Invalid = 0xFFFFFFFF };

using BufferHandle = ResourceHandle;
using ImageHandle = ResourceHandle;

// Temp. idk im tired where put
struct Viewport {
	VkOffset2D Offset;
	VkExtent2D Size;
};

namespace VkUtil {
	struct QueueFamilyIndices_t {
		uint32_t m_GraphicsFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		uint32_t m_PresentFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		uint32_t m_ComputeFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		bool IsValid() const {
			return m_GraphicsFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
				m_ComputeFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
				m_PresentFamilyIndex != VK_QUEUE_FAMILY_IGNORED;
		}
	};

	struct SwaphchainSupportData_t {
		VkSurfaceCapabilitiesKHR m_SurfaceCapabilities{};
		std::vector<VkSurfaceFormatKHR> m_SurfaceFormats{};
		std::vector<VkPresentModeKHR> m_PresentModes{};
	};

	VkInstance CreateInstance( const char** requiredExtensions, const int requiredExtensionsCount );
	VkPhysicalDevice GetPhysicalDevice( const VkInstance& instance, const std::vector<const char*>& wantedExtensions );
	VkDevice CreateLogicalDevice( const VkInstance& instance, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const std::vector<const char*>& wantedExtensions );

	VkSurfaceKHR CreateSurfaceForWindow( const VkInstance& instance, const HWND& hwnd );

	// Physical Device Helpers...
	bool PhysicalDeviceIsDiscreteGPU( const VkPhysicalDevice& physicalDevice );
	bool PhysicalDeviceHasExtensions( const VkPhysicalDevice& physicalDevice, const std::vector<const char*>& wantedExtensions );
	VkFormat PhysicalDeviceFindDepthFormat(const VkPhysicalDevice& physicalDevice );

	// Surface & Physical Device Helpers...
	QueueFamilyIndices_t FindQueueFamilyIndices( const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice );
	SwaphchainSupportData_t QuerySwapchainSupport( const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice );

	// Device Helpers...
	VkQueue DeviceGetQueueByIndex( const VkDevice& device, uint32_t queueIndex );
	VkCommandPool CreateCommandPool( const VkDevice& device, const VkCommandPoolCreateInfo& createInfo );
	VkCommandBuffer CreateCommandBuffer( const VkDevice& device, const VkCommandPool& commandPool );

	// Swapchain Helpers...
	VkSwapchainKHR CreateSwapchain( const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const HWND& windowHandle, VkExtent2D& outExtents );
	VkSurfaceFormatKHR SwapchainSelectSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats );
	VkPresentModeKHR SwapchainSelectPresentMode( const std::vector<VkPresentModeKHR>& availableModes );
	VkExtent2D SwapchainGetExtents( const HWND& windowHandle, const VkSurfaceCapabilitiesKHR& capabilities );
	VkFormat SwapchainGetImageFormat( const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface );

	void CreateSwapchainImages( const VkDevice& device, const VkSwapchainKHR& swapchain, const VkFormat imageFormat, std::vector<VkImage>& outImages, std::vector<VkImageView>& outImageViews );

	VkImageView CreateImageView( const VkDevice& device, const VkImage& image, VkImageViewCreateInfo* createInfo );

	// Command Buffer Helpers...
	// TODO: Make a good CommandBuffer abstraction 
	void CommandBufferBegin( const VkCommandBuffer& commandBuffer, VkCommandBufferUsageFlags flags = 0 );
	void CommandBufferEnd( const VkCommandBuffer& commandBuffer );
	void CommandBufferSubmit( const VkCommandBuffer& commandBuffer, const VkQueue& queue, bool wait = true );
	void CommandBufferCopyBuffer(const VkCommandBuffer& commandBuffer, const VkBuffer& source, const VkBuffer& destination, const VkDeviceSize& size );
	void CommandBufferCopyBufferToImage( const VkCommandBuffer& commandBuffer, const VkBuffer& buffer, const VkImage& image, uint32_t width, uint32_t height );
	void CommandBufferImageBarrier( const VkCommandBuffer& commandBuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange );
	void CommandBufferBufferBarrier( const VkCommandBuffer& commandBuffer, VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask );
	void CommandBufferStageBarrier( const VkCommandBuffer& commandBuffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask );

	void CommandBufferBeginRendering( const VkCommandBuffer& commandBuffer, VkRenderingInfo* renderingInfo );
	void CommandBufferEndRendering( const VkCommandBuffer& commandBuffer );
	void CommandBufferSetScissorAndViewport( const VkCommandBuffer& commandBuffer, float width, float height, float x = 0.f, float y = 0.f, float minDepth = 0.f, float maxDepth = 1.f, bool flip = true );

	// Render Pass Helpers...
	VkRenderingAttachmentInfo RenderPassGetColorAttachmentInfo( const VkImageView view, VkClearValue* clear, VkImageLayout layout, VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE, VkImageView resolveImage = VK_NULL_HANDLE, VkImageLayout resolveLayout = VK_IMAGE_LAYOUT_UNDEFINED );
	VkRenderingAttachmentInfo RenderPassGetDepthAttachmentInfo( const VkImageView view, VkImageLayout layout );
	VkRenderingInfo RenderPassCreateRenderingInfo( VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachments, VkRenderingAttachmentInfo* depthAttachment, uint32_t colorAttachmentCount = 1 );

	// Pipeline Defaults...
	VkPipelineMultisampleStateCreateInfo PipelineDefaultMultiSampleState();
	VkPipelineInputAssemblyStateCreateInfo PipelineDefaultInputAssemblyState();
	VkPipelineRasterizationStateCreateInfo PipelineDefaultRasterizationState();
	VkPipelineDepthStencilStateCreateInfo PipelineDefaultDepthStencilState();
	VkPipelineDynamicStateCreateInfo PipelineDefaultDynamicState();
	VkPipelineColorBlendAttachmentState PipelineDefaultColorBlendAttachmentState();
}