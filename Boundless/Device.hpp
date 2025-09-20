#pragma once
#include "VkUtil.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace Boundless {
	class Device {
	public:
		Device() = default;
		Device(HWND windowHandle );
		~Device();

		VkSampler CreateSampler( const SamplerDesc& samplerDesc );

		Image CreateImage( const Image::Desc& imageDesc );
		ImageHandle CreateTexture( const VkImageView& texture, const VkSampler& sampler );
		BufferHandle CreateBuffer( const Buffer::Desc& bufferDesc );
		Buffer* GetBuffer( BufferHandle handle );

		void ReleaseAllBuffers();
		void ReleaseAllImages();

		VmaAllocator GetAllocator() { return m_Allocator; }
		VkSurfaceKHR GetSurface() { return m_Surface; }
		VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; }
		VkInstance GetInstance() {return m_Instance; }
		VkDevice GetDevice() { return m_Device; }

		VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() { return m_PresentQueue; }
		VkQueue GetComputeQueue() { return m_ComputeQueue; }

		VkCommandPool GetCommandPool() { return m_CommandPool; }

		VkDescriptorPool GetDescriptorPool() { return m_DescriptorPool; }
		VkDescriptorSet GetTexturePool() {return m_TexturePool; }
		VkDescriptorSetLayout GetTexturePoolLayout() {return m_TexturePoolLayout; }

		std::unique_ptr<StagingBuffer> CreateStagingBuffer( const VkDeviceSize size );

		void TransitionImageLayout( const VkImage& image, uint32_t levels, const VkFormat format, const VkImageLayout oldLayout, const VkImageLayout newLayout );
		void GenerateMipmaps( const VkImage& image, uint32_t width, uint32_t height, uint32_t mipLevels );

		Image LoadImageFromFile( const std::string& path, bool isSRGB );

		VkCommandBuffer CreateCommandBuffer();
	private:
		void CreateGlobalDescriptors();

		VkInstance m_Instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;
		VmaAllocator m_Allocator{};

		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VkQueue m_ComputeQueue = VK_NULL_HANDLE;

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;

		// Descriptors...
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_TexturePoolLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_TexturePool = VK_NULL_HANDLE;

		std::vector<VkImageView> m_AllTextures{ VK_NULL_HANDLE };
		std::vector<Buffer*> m_AllBuffers{ nullptr };
		std::vector<Image> m_AllImages{};

		std::vector<std::pair<VkSampler, SamplerDesc>> m_AllSamplers{};
	};
}