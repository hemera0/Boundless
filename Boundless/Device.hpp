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
		Device() = default;
		Device(HWND windowHandle);
		~Device();

		VmaAllocator GetAllocator() { return m_Allocator; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkInstance GetInstance() const { return m_Instance; }
		VkDevice GetDevice() const { return m_Device; }
		VkCommandPool GetCommandPool() const { return m_CommandPool; }
		VkDescriptorPool GetDescriptorPool() const { return m_DescriptorPool; }

		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }
		VkQueue GetComputeQueue() const { return m_ComputeQueue; }

		VkDescriptorSet GetGlobalResources() const { return m_GlobalResources; }
		VkDescriptorSetLayout GetGlobalResourceLayout() const { return m_GlobalResourceLayout; }
		VkPipelineLayout GetGlobalPipelineLayout() const { return m_GlobalPipelineLayout; }

		std::unique_ptr<StagingBuffer> CreateStagingBuffer( const VkDeviceSize size );

		void TransitionImageLayout( const VkImage& image, uint32_t levels, const VkFormat format, const VkImageLayout oldLayout, const VkImageLayout newLayout );
		void GenerateMipmaps( const VkImage& image, uint32_t width, uint32_t height, uint32_t mipLevels );

		// These should be moved to some assets class.
		std::pair<ImageHandle, ImageHandle> LoadImageFromFile( const std::string& path, bool isSRGB );
		std::pair<ImageHandle, ImageHandle> LoadKTXImageFromFile( const std::string& path );

		VkSampler GetSampler( ESamplerType samplerType ) const { return m_Samplers[ samplerType ]; }

		// Resource functions.
		ImageHandle CreateImage( const Image::Desc& imageDesc );
		ImageHandle CreateImageView( ImageHandle resource, const Image::Desc& imageDesc );
		ImageHandle CreateImageView( ImageHandle resource );
		BufferHandle CreateBuffer( const Buffer::Desc& bufferDesc );

		Buffer& GetBuffer( BufferHandle handle ) { return m_AllBuffers[ size_t( handle ) ]; }
		Image& GetImage( ImageHandle handle ) { return m_AllImages[ size_t( handle ) ]; }

		VkSampler CreateSampler( const SamplerDesc& samplerDesc );

		void ReleaseImage( ImageHandle handle );
		void ReleaseImageView( ImageHandle handle );
	private:
		void UploadImageToGPU( VkImageView imageView, uint32_t slotId );

		void CreateSamplers();
		void CreateGlobalDescriptors();

		VkDevice								m_Device;
		VkInstance								m_Instance;
		VkSurfaceKHR							m_Surface;
		VmaAllocator							m_Allocator;
		VkPhysicalDevice						m_PhysicalDevice;
		VkQueue									m_GraphicsQueue;
		VkQueue									m_PresentQueue;
		VkQueue									m_ComputeQueue;
		VkCommandPool							m_CommandPool;
		VkDescriptorPool						m_DescriptorPool;
		VkDescriptorSetLayout					m_GlobalResourceLayout;
		VkDescriptorSet							m_GlobalResources;
		VkPipelineLayout						m_GlobalPipelineLayout;
		std::array<VkSampler, SAMPLER_TYPE_MAX> m_Samplers;

		// TODO: Move to resources class.
		std::vector<Image>	m_AllImages{ };
		std::vector<Buffer> m_AllBuffers{ };
	};
}