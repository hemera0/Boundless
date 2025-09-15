#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	class Image {
		VkDevice m_Device = VK_NULL_HANDLE;
		VkImage m_Image = VK_NULL_HANDLE;
		VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;

		VkFormat m_Format{};
		VkImageViewType m_ViewType{};
		
		uint32_t m_LevelCount{};

		uint32_t m_Width{}, m_Height{};

	public:
		Image(const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits sampleCount, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateInfo* optionalCI = nullptr);
		~Image();

		static VkImageCreateInfo GetDefault2DCreateInfo(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits sampleCount, VkImageUsageFlags usage);

		static Image* LoadFromFile(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkCommandPool& commandPool, const VkQueue& graphicsQueue, const std::string& path);

		void TransitionLayout(const VkCommandPool& commandPool, const VkQueue& graphicsQueue, const VkFormat format, const VkImageLayout oldLayout, const VkImageLayout newLayout);
		void GenerateMipmaps(const VkCommandPool& commandPool, const VkQueue& graphicsQueue, uint32_t width, uint32_t height, uint32_t mipLevels);

		const VkFormat GetFormat() const { return m_Format; }
		const VkImageViewType GetViewType() const { return m_ViewType; }

		// const uint32_t GetLayerCount() const { return m_LayerCount; }
		const uint32_t GetLevelCount() const { return m_LevelCount; }

		const uint32_t GetWidth() const { return m_Width; }
		const uint32_t GetHeight() const { return m_Height; }

		operator VkImage() {
			return m_Image;
		}
	};
}