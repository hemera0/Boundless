#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	struct SamplerDesc {
		VkFilter m_MinFilter{};
		VkFilter m_MagFilter{};
		VkSamplerAddressMode m_WrapS{};
		VkSamplerAddressMode m_WrapT{};
		bool m_Anisotropic = true;
	};

	// Represents a VkImage or VkImageView...
	class Image {
		friend class Device;
	public:
		struct Desc {
			VkImageType			  m_Type = VK_IMAGE_TYPE_2D;
			uint32_t			  m_Width = 1;
			uint32_t			  m_Height = 1;
			uint32_t			  m_Levels = 1;
			uint32_t			  m_Layers = 1;
			VkFormat			  m_Format = VK_FORMAT_UNDEFINED; 
			VkImageTiling		  m_Tiling = VK_IMAGE_TILING_OPTIMAL;
			VkSampleCountFlagBits m_Samples = VK_SAMPLE_COUNT_1_BIT;
			VkImageUsageFlags     m_Usage;
			VkImageLayout		  __Layout = VK_IMAGE_LAYOUT_UNDEFINED; 

			bool operator==( const Desc& other ) const {
				return std::tie( m_Type, m_Width, m_Height, m_Levels, m_Layers, m_Format, m_Tiling, m_Samples, m_Usage ) ==
					std::tie( other.m_Type, other.m_Width, other.m_Height, other.m_Levels, other.m_Layers, other.m_Format, other.m_Tiling, other.m_Samples, other.m_Usage );
			}
		};

		const VkFormat GetFormat() const { return m_Desc.m_Format; }
		const uint32_t GetLayers() const { return m_Desc.m_Layers; }
		const uint32_t GetLevels() const { return m_Desc.m_Levels; }
		const uint32_t GetWidth() const { return m_Desc.m_Width; }
		const uint32_t GetHeight() const { return m_Desc.m_Height; }

		const VkImageUsageFlags GetUsage() const { return m_Desc.m_Usage; }
		const VkImageLayout GetLayout() const {return m_Desc.__Layout; }

		operator VkImage() {
			return m_Image;
		}

		operator VkImageView() {
			return m_ImageView;
		}

		const ResourceHandle GetResourceHandle() const { return m_Resource; }

	private:
		ResourceHandle m_Resource;

		union {
			VkImage		m_Image;
			VkImageView m_ImageView;
		};

		VmaAllocation m_Allocation{};
		Image::Desc   m_Desc{};
	};
}