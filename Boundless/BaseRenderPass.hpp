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
			std::vector<vk::RenderingAttachmentInfo> colorAttachments = {};
			vk::RenderingAttachmentInfo depthAttachment = {};

			if( m_DepthTargetView != ImageHandle::Invalid ) {
				const vk::ImageView& depthView = device.GetImage( m_DepthTargetView );
				depthAttachment = vk_util::RenderPassGetDepthAttachmentInfo( depthView, vk::ImageLayout::eAttachmentOptimal );
			}
			
			colorAttachments.resize( m_RenderTargets.size() );

			for(size_t i = 0; i < colorAttachments.size(); i++ ) {
				const vk::ImageView& colorView = device.GetImage( m_RenderTargetViews[i] );
				
				vk::ClearValue clearValue = { { 0.1f, 0.1f, 0.1f, 1.f } };
				colorAttachments[i] = vk_util::RenderPassGetColorAttachmentInfo( colorView, &clearValue, vk::ImageLayout::eAttachmentOptimal );
			}

			vk::RenderingAttachmentInfo* depthInfo = m_DepthTargetView == ImageHandle::Invalid ? nullptr : &depthAttachment;
			vk::RenderingInfo renderingInfo = vk_util::RenderPassCreateRenderingInfo( m_Viewport.Size, colorAttachments.data(), depthInfo, uint32_t( colorAttachments.size() ) );

			commandBuffer.BeginRendering(renderingInfo);
		}

		void EndRendering( CommandBuffer& commandBuffer, [[maybe_unused]] Device& device ) {
			commandBuffer.EndRendering();
		}

		void AddExitBarriers( CommandBuffer& commandBuffer, Device& device ) {
			std::vector<Image::Desc> allDescs = m_RenderTargetDescs;
			if( m_DepthDesc.m_Format != vk::Format::eUndefined )
				allDescs.push_back(m_DepthDesc);

			for(size_t i = 0; i < allDescs.size(); i++ ) {
				const Image::Desc& desc = allDescs[i];
				if( !( desc.m_Usage & vk::ImageUsageFlagBits::eSampled ) ) // If we don't plan to sample the image ignore this barrier.
					continue;

				bool isDepth = ( desc.m_Usage & vk::ImageUsageFlagBits::eDepthStencilAttachment ) == vk::ImageUsageFlagBits::eDepthStencilAttachment;

				vk::AccessFlags srcAccessMask = isDepth ? ( vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite ) : ( vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite );
				vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eShaderRead;

				vk::PipelineStageFlags srcStageMask = isDepth ? ( vk::PipelineStageFlagBits::eLateFragmentTests ) : ( vk::PipelineStageFlagBits::eColorAttachmentOutput );
				vk::ImageSubresourceRange subresourceRange = isDepth ? vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers } : 
					vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers };
			
				ImageHandle resource = isDepth ? m_DepthTarget : m_RenderTargets[i];
				const vk::Image& image = device.GetImage(resource);

				commandBuffer.ImageBarrier( 
					image,
					srcAccessMask,
					dstAccessMask,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eShaderReadOnlyOptimal,
					srcStageMask,
					vk::PipelineStageFlagBits::eFragmentShader,
					subresourceRange
				);
			}
		}

		virtual void CreatePassResources( Device& device ) {
			if ( m_DepthDesc.m_Format != vk::Format::eUndefined ) {
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

			std::vector<vk::Format> renderTargetFormats = m_RenderTargetDescs | std::views::transform( &Image::Desc::m_Format ) | std::ranges::to<std::vector>();

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
		ComputePipelineBuilder	 m_ComputePipelineBuilder = {};
		PipelineBuilder			 m_PipelineBuilder = {};
		vk::Pipeline			 m_Pipeline;
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