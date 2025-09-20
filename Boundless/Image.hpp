#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	struct SamplerDesc {
		VkFilter m_MinFilter{};
		VkFilter m_MagFilter{};
		VkSamplerAddressMode m_WrapS{};
		VkSamplerAddressMode m_WrapT{};
	};

	class Image {
		friend class Device;
	public:
		struct Desc {
			uint32_t m_Width;
			uint32_t m_Height;
			uint32_t m_Levels;
			VkFormat m_Format; 
			VkImageTiling m_Tiling;
			VkSampleCountFlagBits m_Samples;
			VkImageUsageFlags m_Usage;
		};

		VkImageView GetView() const {return m_ImageView; }

		const VkFormat GetFormat() const { return m_Format; }

		const uint32_t GetLayers() const { return m_Layers; }
		const uint32_t GetLevels() const { return m_Levels; }

		const uint32_t GetWidth() const { return m_Width; }
		const uint32_t GetHeight() const { return m_Height; }

		operator VkImage() {
			return m_Image;
		}
	private:
		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VmaAllocation m_Allocation{};

		VkFormat m_Format{};
		uint32_t m_Levels{}, m_Layers{}, m_Width{}, m_Height{};
	};
}