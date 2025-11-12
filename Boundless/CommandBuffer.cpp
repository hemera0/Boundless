#include "Pch.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"

namespace Boundless {
	CommandBuffer::CommandBuffer( Device& device ) { 
		vk::CommandBufferAllocateInfo allocateInfo = {};
		allocateInfo.commandPool = device.GetCommandPool();
		allocateInfo.level = vk::CommandBufferLevel::ePrimary;
		allocateInfo.commandBufferCount = 1;
		m_CommandBuffer = device->allocateCommandBuffers( allocateInfo )[ 0 ];
	}

	CommandBuffer::CommandBuffer( Device& device, const vk::CommandPool& commandPool ) { 
		vk::CommandBufferAllocateInfo allocateInfo = {};
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = vk::CommandBufferLevel::ePrimary;
		allocateInfo.commandBufferCount = 1;
		m_CommandBuffer = device->allocateCommandBuffers( allocateInfo )[ 0 ];
	}
	
	void CommandBuffer::Begin( vk::CommandBufferUsageFlagBits flags ) {
		vk::CommandBufferBeginInfo commandBufferBeginInfo = {};

		if ( flags != vk::CommandBufferUsageFlags{} )
			commandBufferBeginInfo.flags = flags;

		m_CommandBuffer.begin( commandBufferBeginInfo );
	}
	
	void CommandBuffer::End() {
		m_CommandBuffer.end();
	}
	
	void CommandBuffer::Submit( const vk::Queue& queue, bool wait ) { 
		vk::SubmitInfo submitInfo = {};
		submitInfo.setCommandBuffers( { m_CommandBuffer } );

		queue.submit(submitInfo);

		if ( wait )
			queue.waitIdle();
	}
	
	void CommandBuffer::CopyBuffer( const vk::Buffer& source, const vk::Buffer& destination, const vk::DeviceSize& size ) {
		vk::BufferCopy bufferCopy = {};
		bufferCopy.srcOffset = 0;
		bufferCopy.dstOffset = 0;
		bufferCopy.size = size;

		m_CommandBuffer.copyBuffer(source, destination, { bufferCopy } );
	}
	
	void CommandBuffer::CopyBufferToImage( const vk::Buffer& buffer, const vk::Image& image, uint32_t width, uint32_t height ) {
		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
		region.imageOffset = vk::Offset3D{ 0, 0, 0 };
		region.imageExtent = vk::Extent3D{ width, height, 1 };

		m_CommandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region } );
	}
	
	void CommandBuffer::ImageBarrier( const vk::Image& image, vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask, vk::ImageSubresourceRange subresourceRange ) {
		vk::ImageMemoryBarrier barrier = {};
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.oldLayout = oldImageLayout;
		barrier.newLayout = newImageLayout;
		barrier.image = image;
		barrier.subresourceRange = subresourceRange;

		m_CommandBuffer.pipelineBarrier( srcStageMask, dstStageMask, {}, {}, {}, { barrier } );
	}
	
	void CommandBuffer::StageBarrier( vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask ) {
		vk::MemoryBarrier2 memoryBarrier = {};
		memoryBarrier.srcStageMask = srcStageMask;
		memoryBarrier.srcAccessMask = srcAccessMask;
		memoryBarrier.dstStageMask = dstStageMask;
		memoryBarrier.dstAccessMask = dstAccessMask;

		vk::DependencyInfo dependencyInfo = {};
		dependencyInfo.setMemoryBarriers( { memoryBarrier });

		m_CommandBuffer.pipelineBarrier2( dependencyInfo );
	}
	
	void CommandBuffer::BeginRendering( const vk::RenderingInfo& renderingInfo ) {
		m_CommandBuffer.beginRendering( renderingInfo);
	}
	
	void CommandBuffer::EndRendering() { 
		m_CommandBuffer.endRendering();
	}
	
	void CommandBuffer::SetScissorAndViewport( float width, float height, float x, float y, float minDepth, float maxDepth, bool flip ) { 
		vk::Rect2D scissor = { { int32_t( x ), int32_t( y ) }, { uint32_t( width ),uint32_t( height ) } };
		m_CommandBuffer.setScissor( 0, { scissor } );

		vk::Viewport viewport{};
		viewport.x = x;
		viewport.y = flip ? height : y;
		viewport.width = width;
		viewport.height = flip ? -height : height; // flip y
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		
		m_CommandBuffer.setViewport( 0, { viewport } );
	}

	void CommandBuffer::SetScissorAndViewport( Image& image ) {
		SetScissorAndViewport( float( image.GetWidth() ), float( image.GetHeight() ) );
	}

	void CommandBuffer::SetScissorAndViewport( const Viewport& viewport ) { 
		SetScissorAndViewport( float( viewport.Size.width ), float( viewport.Size.height ) );
	}

	void CommandBuffer::BindDefaults( Device& device ) {
		vk::DescriptorSet globalResources = device.GetGlobalResources();
		vk::PipelineLayout pipelineLayout = device.GetGlobalPipelineLayout();
		
		//vkCmdBindDescriptorSets( m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &texturePool, 0, nullptr );
		m_CommandBuffer.bindDescriptorSets( vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, { globalResources }, {} );
		
		// Just set this here for now.
		m_CommandBuffer.setFrontFace( vk::FrontFace::eCounterClockwise );	
	}

	void CommandBuffer::BindPipeline( const vk::Pipeline& pipeline ) {
		m_CommandBuffer.bindPipeline( vk::PipelineBindPoint::eGraphics, pipeline );
	}

	void CommandBuffer::BindComputePipeline( const vk::Pipeline& pipeline ) { 
		m_CommandBuffer.bindPipeline( vk::PipelineBindPoint::eCompute, pipeline );
	}

	void CommandBuffer::BindIndexBuffer( Buffer& buffer, vk::IndexType indexType ) {
		m_CommandBuffer.bindIndexBuffer( buffer, 0, indexType );
	}
	
	void CommandBuffer::BindPushConstants( Device& device, void* value, size_t size ) {
		vk::PipelineLayout pipelineLayout = device.GetGlobalPipelineLayout();
		m_CommandBuffer.pushConstants( pipelineLayout, vk::ShaderStageFlagBits::eAll, 0, uint32_t( size ), value );
	}
}