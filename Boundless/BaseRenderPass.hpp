#pragma once
#include "VkUtil.hpp"
#include "Device.hpp"
#include "Pipelines.hpp"

namespace Boundless {
	class BaseRenderPass {
	public:
		BaseRenderPass( const Viewport& viewport, const std::string& name ) : m_Viewport( viewport ), m_Name( name ) { }

		void DepthTarget( const Image::Desc& desc ) { m_DepthDesc = desc; }
		void RenderTarget( const Image::Desc& desc ) { m_RenderTargetDescs.push_back(desc); }

		void BeginRendering(CommandBuffer& commandBuffer, Device& device) {
			std::vector<VkRenderingAttachmentInfo> colorAttachments = {};
			VkRenderingAttachmentInfo depthAttachment = {};

			if( m_DepthTargetView != ImageHandle::Invalid ) {
				VkImageView depthView = device.GetImage( m_DepthTargetView );
				depthAttachment = VkUtil::RenderPassGetDepthAttachmentInfo( depthView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
			}
			
			colorAttachments.resize( m_RenderTargets.size() );

			for(size_t i = 0; i < colorAttachments.size(); i++ ) {
				VkImageView colorView = device.GetImage( m_RenderTargetViews[i] );
				
				VkClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
				colorAttachments[i] = VkUtil::RenderPassGetColorAttachmentInfo( colorView, &clearValue, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL );
			}

			VkRenderingAttachmentInfo* depthInfo = m_DepthTargetView == ImageHandle::Invalid ? nullptr : &depthAttachment;
			VkRenderingInfo renderingInfo = VkUtil::RenderPassCreateRenderingInfo( m_Viewport.Size, colorAttachments.data(), depthInfo, uint32_t(colorAttachments.size()) );

			commandBuffer.BeginRendering(&renderingInfo);
		}

		void EndRendering( CommandBuffer& commandBuffer, [[maybe_unused]] Device& device ) {
			commandBuffer.EndRendering();
		}

		void AddExitBarriers( CommandBuffer& commandBuffer, Device& device ) {
			std::vector<Image::Desc> allDescs = m_RenderTargetDescs;
			if( m_DepthDesc.m_Format != VK_FORMAT_UNDEFINED )
				allDescs.push_back(m_DepthDesc);

			for(size_t i = 0; i < allDescs.size(); i++ ) {
				const Image::Desc& desc = allDescs[i];
				if( !( desc.m_Usage & VK_IMAGE_USAGE_SAMPLED_BIT ) ) // If we don't plan to sample the image ignore this barrier.
					continue;

				bool isDepth = desc.m_Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

				VkAccessFlags srcAccessMask = isDepth ? ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) : ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
				VkAccessFlags dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				VkPipelineStageFlags srcStageMask = isDepth ? ( VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ) : ( VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
				VkImageSubresourceRange subresourceRange = isDepth ? VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } : VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			
				ImageHandle resource = isDepth ? m_DepthTarget : m_RenderTargets[i];
				VkImage image = device.GetImage(resource);

				commandBuffer.ImageBarrier( 
					image,
					srcAccessMask,
					dstAccessMask,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					srcStageMask,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					subresourceRange
				);
			}
		}

		virtual void CreatePassResources( Device& device ) {
			if ( m_DepthDesc.m_Format != VK_FORMAT_UNDEFINED ) {
				m_DepthTarget = device.CreateImage( m_DepthDesc );
				m_DepthTargetView = device.CreateImageView( m_DepthTarget );
				
				m_PipelineBuilder.SetDepthAttachmentFormat( m_DepthDesc.m_Format );
			}

			m_RenderTargets.resize( m_RenderTargetDescs.size() );
			m_RenderTargetViews.resize( m_RenderTargetDescs.size() );

			for( size_t i = 0; i < m_RenderTargetDescs.size(); i++ ) {
				m_RenderTargets[ i ] = device.CreateImage( m_RenderTargetDescs[ i ] );
				m_RenderTargetViews[ i ] = device.CreateImageView( m_RenderTargets[ i ] );
			}

			std::vector<VkFormat> renderTargetFormats = m_RenderTargetDescs | std::views::transform( &Image::Desc::m_Format ) | std::ranges::to<std::vector>();

			m_PipelineBuilder
				.SetColorAttachmentFormats( renderTargetFormats )
				.SetPipelineLayout( device.GetGlobalPipelineLayout() );
		}

		virtual void ReleasePassResources( Device& device ) {
			if ( m_DepthTargetView != ImageHandle::Invalid ) {
				device.ReleaseImageView( m_DepthTargetView );
			}

			if ( m_DepthTarget != ImageHandle::Invalid ) {
				device.ReleaseImage( m_DepthTarget );
			}

			for ( size_t i = 0; i < m_RenderTargetDescs.size(); i++ ) {
				if ( m_RenderTargetViews[i] != ImageHandle::Invalid )
					device.ReleaseImageView( m_RenderTargetViews[ i ] );
				
				if ( m_RenderTargets[ i ] != ImageHandle::Invalid )
					device.ReleaseImage( m_RenderTargets[ i ] );
			}
		}

		virtual ImageHandle GetDepthBuffer() const { return m_DepthTarget; }
		virtual ImageHandle GetDepthBufferView() const { return m_DepthTargetView; }
		virtual ImageHandle GetRenderTarget() const { return m_RenderTargets[ 0 ]; }
		virtual ImageHandle GetRenderTargetView() const { return m_RenderTargetViews[ 0 ]; }
	protected:
		PipelineBuilder			 m_PipelineBuilder = {};
		VkPipeline				 m_Pipeline;
		std::vector<Image::Desc> m_RenderTargetDescs;
		std::vector<ImageHandle> m_RenderTargetViews;
		std::vector<ImageHandle> m_RenderTargets;
		ImageHandle				 m_DepthTargetView = ImageHandle::Invalid;
		ImageHandle				 m_DepthTarget = ImageHandle::Invalid;
		Image::Desc				 m_DepthDesc;
		Viewport				 m_Viewport;
		std::string				 m_Name;
	};
}