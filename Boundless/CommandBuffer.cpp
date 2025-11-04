#include "Pch.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"

namespace Boundless {
	// TODO: Slowly remove all the VkUtil functions and implement them here.
	CommandBuffer::CommandBuffer( Device& device ) { 
		m_CommandBuffer = VkUtil::CreateCommandBuffer( device.GetDevice(), device.GetCommandPool() );
	}
	
	void CommandBuffer::Begin( VkCommandBufferUsageFlags flags ) { 
		VkUtil::CommandBufferBegin(m_CommandBuffer, flags);
	}
	
	void CommandBuffer::End() { 
		VkUtil::CommandBufferEnd(m_CommandBuffer);
	}
	
	void CommandBuffer::Submit( const VkQueue& queue, bool wait ) { 
		VkUtil::CommandBufferSubmit(m_CommandBuffer, queue, wait);
	}
	
	void CommandBuffer::CopyBuffer( const VkBuffer& source, const VkBuffer& destination, const VkDeviceSize& size ) { 
		VkUtil::CommandBufferCopyBuffer(m_CommandBuffer, source, destination, size);
	}
	
	void CommandBuffer::CopyBufferToImage( const VkBuffer& buffer, const VkImage& image, uint32_t width, uint32_t height ) { 
		VkUtil::CommandBufferCopyBufferToImage(m_CommandBuffer, buffer, image, width, height);
	}
	
	void CommandBuffer::ImageBarrier( VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange ) { 
		VkUtil::CommandBufferImageBarrier(m_CommandBuffer, image, srcAccessMask, dstAccessMask, oldImageLayout, newImageLayout, srcStageMask, dstStageMask, subresourceRange);
	}
	
	void CommandBuffer::BufferBarrier( VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask ) { 
		VkUtil::CommandBufferBufferBarrier( m_CommandBuffer, buffer, srcStageMask, srcAccessMask, dstStageMask, dstAccessMask );
	}
	
	void CommandBuffer::StageBarrier( VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask ) { 
		VkUtil::CommandBufferStageBarrier(m_CommandBuffer, srcStageMask, srcAccessMask, dstStageMask, dstAccessMask );
	}
	
	void CommandBuffer::BeginRendering( VkRenderingInfo* renderingInfo ) { 
		VkUtil::CommandBufferBeginRendering( m_CommandBuffer, renderingInfo );
	}
	
	void CommandBuffer::EndRendering() { 
		VkUtil::CommandBufferEndRendering(m_CommandBuffer);
	}
	
	void CommandBuffer::SetScissorAndViewport( float width, float height, float x, float y, float minDepth, float maxDepth, bool flip ) { 
		VkUtil::CommandBufferSetScissorAndViewport(m_CommandBuffer, width, height, x, y, minDepth, maxDepth, flip);
	}

	void CommandBuffer::SetScissorAndViewport( Image& image ) {
		SetScissorAndViewport( float( image.GetWidth() ), float( image.GetHeight() ) );
	}

	void CommandBuffer::SetScissorAndViewport( const Viewport& viewport ) { 
		SetScissorAndViewport( float( viewport.Size.width ), float( viewport.Size.height ) );
	}

	void CommandBuffer::BindDefaults( Device& device ) {
		VkDescriptorSet texturePool = device.GetGlobalResources();
		VkPipelineLayout pipelineLayout = device.GetGlobalPipelineLayout();
		vkCmdBindDescriptorSets( m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &texturePool, 0, nullptr );

		// Just set this here for now.
		vkCmdSetFrontFace( m_CommandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE );
	}

	void CommandBuffer::BindPipeline( VkPipeline pipeline ) {
		vkCmdBindPipeline( m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	}

	void CommandBuffer::BindIndexBuffer( Buffer& buffer, VkIndexType indexType ) {
		vkCmdBindIndexBuffer( m_CommandBuffer, buffer, 0, indexType );
	}
	
	void CommandBuffer::BindPushConstants( Device& device, void* value, size_t size ) {
		VkPipelineLayout pipelineLayout = device.GetGlobalPipelineLayout();
		vkCmdPushConstants( m_CommandBuffer, pipelineLayout, VK_SHADER_STAGE_ALL, 0, uint32_t(size), value);
	}
}