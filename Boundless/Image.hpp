#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	struct SamplerDesc {
		vk::Filter m_MinFilter{};
		vk::Filter m_MagFilter{};
		vk::SamplerAddressMode m_WrapS{};
		vk::SamplerAddressMode m_WrapT{};
		bool m_Anisotropic = true;
	};

	class Image {
		friend class Device;
	public:
		struct Desc {
			vk::ImageType			m_Type = vk::ImageType::e2D;
			uint32_t				m_Width = 1;
			uint32_t				m_Height = 1;
			uint32_t				m_Levels = 1;
			uint32_t				m_Layers = 1;
			vk::Format				m_Format = vk::Format::eUndefined; 
			vk::ImageTiling			m_Tiling = vk::ImageTiling::eOptimal; // I have never not used optimal...
			vk::SampleCountFlagBits m_Samples = vk::SampleCountFlagBits::e1;
			vk::ImageUsageFlags     m_Usage;
			vk::ImageLayout			__Layout = vk::ImageLayout::eUndefined; 

			bool operator==( const Desc& other ) const {
				return std::tie( m_Type, m_Width, m_Height, m_Levels, m_Layers, m_Format, m_Tiling, m_Samples, m_Usage ) ==
					std::tie( other.m_Type, other.m_Width, other.m_Height, other.m_Levels, other.m_Layers, other.m_Format, other.m_Tiling, other.m_Samples, other.m_Usage );
			}
		};

		Image() { }; // temp fix for vk::Image ctor.

		operator vk::Image& ( ) { return m_Image; }
		operator const vk::Image& ( ) const { return m_Image; }
		operator vk::ImageView& ( ) { return m_ImageView; }
		operator const vk::ImageView& ( ) const { return m_ImageView; }

		vk::Format GetFormat() const { return m_Desc.m_Format; }
		vk::ImageUsageFlags GetUsage() const { return m_Desc.m_Usage; }
		vk::ImageLayout GetLayout() const { return m_Desc.__Layout; }

		uint32_t GetLayers() const { return m_Desc.m_Layers; }
		uint32_t GetLevels() const { return m_Desc.m_Levels; }
		uint32_t GetWidth() const { return m_Desc.m_Width; }
		uint32_t GetHeight() const { return m_Desc.m_Height; }
		
		ResourceHandle GetResourceHandle() const { return m_Resource; }
	private:
		ResourceHandle m_Resource;

		union {
			vk::Image	  m_Image;
			vk::ImageView m_ImageView;
		};

		VmaAllocation m_Allocation{};
		Image::Desc   m_Desc{};
	};
}