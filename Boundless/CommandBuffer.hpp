#pragma once
#include "VkUtil.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace Boundless {
	class CommandBuffer {
		friend class Device;
	public:
		CommandBuffer() = default;
		CommandBuffer( Device& device );
		CommandBuffer( Device& device, const vk::CommandPool& commandPool );

		operator vk::CommandBuffer&() { return m_CommandBuffer; }
		operator const vk::CommandBuffer&() const { return m_CommandBuffer; }
		vk::CommandBuffer* operator->() { return &m_CommandBuffer; }
		const vk::CommandBuffer* operator->() const { return &m_CommandBuffer; }

		void Begin( vk::CommandBufferUsageFlagBits flags = {} );
		void End();
		void Submit( const vk::Queue& queue, bool wait = true );
		void CopyBuffer( const vk::Buffer& source, const vk::Buffer& destination, const vk::DeviceSize& size );
		void CopyBufferToImage( const vk::Buffer& buffer, const vk::Image& image, uint32_t width, uint32_t height );
		void ImageBarrier( const vk::Image& image, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, vk::ImageSubresourceRange subresourceRange );
		// void BufferBarrier( const vk::Buffer& buffer, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask );
		void StageBarrier( vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask );

		void BeginRendering( const vk::RenderingInfo& renderingInfo );
		void EndRendering();
		
		void SetScissorAndViewport( float width, float height, float x = 0.f, float y = 0.f, float minDepth = 0.f, float maxDepth = 1.f, bool flip = true );
		void SetScissorAndViewport( Image& image );
		void SetScissorAndViewport( const Viewport& viewport );

		void BindDefaults( Device& device );
		void BindPipeline( const vk::Pipeline& pipeline );
		void BindComputePipeline( const vk::Pipeline& pipeline );

		void BindIndexBuffer( Buffer& buffer, vk::IndexType indexType = vk::IndexType::eUint32 );
		void BindPushConstants( Device& device, void* value, size_t size );
	private:
		vk::CommandBuffer m_CommandBuffer;
	};
}