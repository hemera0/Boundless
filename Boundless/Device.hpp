#pragma once
#include "VkUtil.hpp"
#include "Image.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"

namespace Boundless {
	// TODO: Move to resources file.
	enum ESamplerType {
		// POINT_WRAP,
		LINEAR_CLAMP = 0,
		ANISO_WRAP,
		ANISO_CLAMP,
		SAMPLER_TYPE_MAX
	};

	class Device {
	public:
		explicit Device(HWND windowHandle);
		~Device();

		operator vk::Device& ( ) { return m_Device; }
		operator const vk::Device& ( ) const { return m_Device; }
		vk::Device* operator->() { return &m_Device; }
		const vk::Device* operator->() const { return &m_Device; }

		VmaAllocator GetAllocator() { return m_Allocator; }
		vk::SurfaceKHR GetSurface() const { return m_Surface; }
		vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		vk::Instance GetInstance() const { return m_Instance; }
		vk::Device GetDevice() const { return m_Device; }
		vk::CommandPool GetCommandPool() const { return m_CommandPool; }
		vk::DescriptorPool GetDescriptorPool() const { return m_DescriptorPool; }

		vk::Queue GetQueue() const { return m_Queue; }
		uint32_t GetQueueIndex() const { return m_QueueIndex; }
		
		vk::DescriptorSet GetGlobalResources() const { return m_GlobalResources; }
		vk::DescriptorSetLayout GetGlobalResourceLayout() const { return m_GlobalResourceLayout; }
		vk::PipelineLayout GetGlobalPipelineLayout() const { return m_GlobalPipelineLayout; }

		std::unique_ptr<StagingBuffer> CreateStagingBuffer( const vk::DeviceSize size );

		void TransitionImageLayout( const vk::Image& image, uint32_t levels, const vk::Format format, const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout );
		void GenerateMipmaps( const vk::Image& image, uint32_t width, uint32_t height, uint32_t mipLevels );

		// These should be moved to some assets class.
		std::pair<ImageHandle, ImageHandle> LoadImageFromFile( const std::string& path, bool isSRGB );
		std::pair<ImageHandle, ImageHandle> LoadKTXImageFromFile( const std::string& path );

		// Resource functions.
		vk::Sampler CreateSampler( const SamplerDesc& samplerDesc );
		vk::Sampler GetSampler( ESamplerType samplerType ) const { return m_Samplers[ samplerType ]; }

		ImageHandle CreateImage( const Image::Desc& imageDesc );
		ImageHandle CreateImageView( ImageHandle resource, const Image::Desc& imageDesc );
		ImageHandle CreateImageView( ImageHandle resource );
		BufferHandle CreateBuffer( const Buffer::Desc& bufferDesc );

		Buffer& GetBuffer( BufferHandle handle ) { return m_AllBuffers[ size_t( handle ) ]; }
		Image& GetImage( ImageHandle handle ) { return m_AllImages[ size_t( handle ) ]; }

		void ReleaseImage( ImageHandle handle );
		void ReleaseImageView( ImageHandle handle );
	private:
		void UploadImageToGPU( const vk::ImageView& imageView, uint32_t slotId );

		void CreateSamplers();
		void CreateGlobalDescriptors();

		vk::Device								  m_Device;
		vk::Instance							  m_Instance;
		vk::SurfaceKHR							  m_Surface;
		VmaAllocator							  m_Allocator;
		vk::PhysicalDevice						  m_PhysicalDevice;
		vk::Queue								  m_Queue;
		uint32_t								  m_QueueIndex;
		vk::CommandPool							  m_CommandPool;
		vk::DescriptorPool						  m_DescriptorPool;
		vk::DescriptorSetLayout					  m_GlobalResourceLayout;
		vk::DescriptorSet						  m_GlobalResources;
		vk::PipelineLayout						  m_GlobalPipelineLayout;
		std::array<vk::Sampler, SAMPLER_TYPE_MAX> m_Samplers;

		// TODO: Move to resources class.
		std::vector<Image>	m_AllImages{ };
		std::vector<Buffer> m_AllBuffers{ };
	};
}