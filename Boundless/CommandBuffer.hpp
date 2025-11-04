#pragma once
#include "VkUtil.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace Boundless {
	class CommandBuffer {
		friend class Device;
	public:
		CommandBuffer() = default;
		CommandBuffer(Device& device);

		void Begin( VkCommandBufferUsageFlags flags = 0 );
		void End( );
		void Submit( const VkQueue& queue, bool wait = true );
		void CopyBuffer( const VkBuffer& source, const VkBuffer& destination, const VkDeviceSize& size );
		void CopyBufferToImage( const VkBuffer& buffer, const VkImage& image, uint32_t width, uint32_t height );
		void ImageBarrier( VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange );
		void BufferBarrier( VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask );
		void StageBarrier( VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask );

		void BeginRendering( VkRenderingInfo* renderingInfo );
		void EndRendering( );
		
		void SetScissorAndViewport( float width, float height, float x = 0.f, float y = 0.f, float minDepth = 0.f, float maxDepth = 1.f, bool flip = true );
		void SetScissorAndViewport( Image& image );
		void SetScissorAndViewport( const Viewport& viewport );

		void BindDefaults( Device& device );
		void BindPipeline( VkPipeline pipeline );
		void BindIndexBuffer( Buffer& buffer, VkIndexType indexType = VK_INDEX_TYPE_UINT32 );
		void BindPushConstants( Device& device, void* value, size_t size );

		operator VkCommandBuffer ( ) { return m_CommandBuffer; }
		operator const VkCommandBuffer () const { return m_CommandBuffer; }
	private:
		VkCommandBuffer m_CommandBuffer;
	};
}