#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	class Buffer {
		friend class Device;
	public:
		struct Desc {
			VkDeviceSize m_Size			 = 0;
			VkBufferUsageFlags m_Usage	 = 0;
			VmaMemoryUsage m_MemoryUsage = VMA_MEMORY_USAGE_AUTO;
			bool m_Mappable				 = false;

			bool operator==(const Desc& other) const {
				return std::tie( m_Size, m_Usage, m_MemoryUsage, m_Mappable) == std::tie( other.m_Size, other.m_Usage, other.m_MemoryUsage, other.m_Mappable );
			}

			bool operator!=( const Desc& other ) const {
				return std::tie( m_Size, m_Usage, m_MemoryUsage, m_Mappable ) != std::tie( other.m_Size, other.m_Usage, other.m_MemoryUsage, other.m_Mappable );
			}
		};

		Buffer( const VkDevice& device, const VmaAllocator& allocator, const Buffer::Desc& bufferDesc );

		void Release();

		void* Map();
		void Unmap();

		void Patch( void* data, size_t size );

		VkBuffer GetHandle() const { return m_Handle; }
		VkDeviceSize GetSize() const { return m_Size; }
		VkDeviceAddress GetDeviceAddress() const;

		operator VkBuffer() const {
			return m_Handle;
		}
	private:
		VkDevice m_Device = VK_NULL_HANDLE;
		VkBuffer m_Handle = VK_NULL_HANDLE;
		VmaAllocator m_Allocator{};
		VmaAllocation m_Allocation{};
		VkDeviceSize m_Size{};
	};

	class StagingBuffer : public Buffer {
	public:
		StagingBuffer( const VkDevice& device, const VmaAllocator& allocator, const VkDeviceSize size ) :
			Buffer( device, allocator, Buffer::Desc{ size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true } ) { }
	};
}