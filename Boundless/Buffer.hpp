#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	class Buffer {
		friend class Device;
	public:
		struct Desc {
			vk::DeviceSize	     m_Size;
			vk::BufferUsageFlags m_Usage;
			VmaMemoryUsage		 m_MemoryUsage = VMA_MEMORY_USAGE_AUTO;
			bool				 m_Mappable = false;

			bool operator==( const Desc& other ) const {
				return std::tie( m_Size, m_Usage, m_MemoryUsage, m_Mappable ) == std::tie( other.m_Size, other.m_Usage, other.m_MemoryUsage, other.m_Mappable );
			}
		};

		explicit Buffer( const vk::Device& device, const VmaAllocator& allocator, const Buffer::Desc& bufferDesc );

		operator vk::Buffer& ( ) { return m_Handle; }
		operator const vk::Buffer& ( ) const { return m_Handle; }

		void Release();

		void* Map();
		void Unmap();

		void Patch( void* data, size_t size );

		vk::Buffer GetHandle() const { return m_Handle; }
		vk::DeviceSize GetSize() const { return m_Size; }
		vk::DeviceAddress GetDeviceAddress() const;


	private:
		vk::Device	  m_Device = VK_NULL_HANDLE;
		vk::Buffer	  m_Handle = VK_NULL_HANDLE;
		
		VmaAllocator  m_Allocator{};
		VmaAllocation m_Allocation{};
		vk::DeviceSize  m_Size{};
	};

	class StagingBuffer : public Buffer {
	public:
		StagingBuffer( const VkDevice& device, const VmaAllocator& allocator, const vk::DeviceSize size ) :
			Buffer( device, allocator, Buffer::Desc{ size, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_AUTO, true } ) { }
	};
}