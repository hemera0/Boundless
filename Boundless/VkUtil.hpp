#pragma once
#include "Pch.hpp"

// Generic Vulkan boilerplate code.
enum class ResourceHandle : uint32_t { Invalid = 0xFFFFFFFF };

using BufferHandle = ResourceHandle;
using ImageHandle = ResourceHandle;

// Temp. idk im tired where put
struct Viewport {
	vk::Offset2D Offset;
	vk::Extent2D Size;
};

namespace vk_util {
	vk::Instance CreateInstance( const char** requiredExtensions, const int requiredExtensionsCount );
	vk::PhysicalDevice GetPhysicalDevice( const vk::Instance& instance, const std::vector<const char*>& wantedExtensions );
	vk::Device CreateLogicalDevice( const vk::Instance& instance, const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const std::vector<const char*>& wantedExtensions );
	vk::SurfaceKHR CreateSurfaceForWindow( const vk::Instance& instance, const HWND& hwnd );

	// Physical Device Helpers...
	bool PhysicalDeviceHasExtensions( const vk::PhysicalDevice& physicalDevice, const std::vector<const char*>& wantedExtensions );

	// Surface & Physical Device Helpers...
	uint32_t FindQueueFamilyIndex( const vk::SurfaceKHR& surface, const vk::PhysicalDevice& physicalDevice );

	// Swapchain Helpers...
	vk::SwapchainKHR CreateSwapchain( const vk::Device& device, const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const HWND& windowHandle, vk::Extent2D& outExtents, vk::Format& outImageFormat );
	vk::SurfaceFormatKHR SwapchainSelectSurfaceFormat( const std::vector<vk::SurfaceFormatKHR>& availableFormats );
	vk::PresentModeKHR SwapchainSelectPresentMode( const std::vector<vk::PresentModeKHR>& availableModes );
	vk::Extent2D SwapchainGetExtents( const HWND& windowHandle, const vk::SurfaceCapabilitiesKHR& capabilities );

	// Render Pass Helpers...
	vk::RenderingAttachmentInfo RenderPassGetColorAttachmentInfo( const vk::ImageView& view, vk::ClearValue* clear, vk::ImageLayout layout, vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone, const vk::ImageView& resolveImage = VK_NULL_HANDLE, vk::ImageLayout resolveLayout = vk::ImageLayout::eUndefined );
	vk::RenderingAttachmentInfo RenderPassGetDepthAttachmentInfo( const vk::ImageView& view, vk::ImageLayout layout );
	vk::RenderingInfo RenderPassCreateRenderingInfo( vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachments, vk::RenderingAttachmentInfo* depthAttachment, uint32_t colorAttachmentCount = 1 );

	// Pipeline Defaults...
	vk::PipelineMultisampleStateCreateInfo PipelineDefaultMultiSampleState();
	vk::PipelineInputAssemblyStateCreateInfo PipelineDefaultInputAssemblyState();
	vk::PipelineRasterizationStateCreateInfo PipelineDefaultRasterizationState();
	vk::PipelineDepthStencilStateCreateInfo PipelineDefaultDepthStencilState();
	vk::PipelineDynamicStateCreateInfo PipelineDefaultDynamicState();
	vk::PipelineColorBlendAttachmentState PipelineDefaultColorBlendAttachmentState();
}