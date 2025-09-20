#pragma once
#include "VkUtil.hpp"

namespace Boundless {
	class Buffer {
		friend class Device;
	public:
		struct Desc {
			VkDeviceSize m_Size{};
			VkBufferUsageFlags m_Usage{};
			VmaMemoryUsage m_MemoryUsage{};
			bool m_Mappable{};
		};

		Buffer( const VkDevice& device, const VmaAllocator& allocator, const VkDeviceSize size, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage, bool mappable = false );
		Buffer( const VkDevice& device, const VmaAllocator& allocator, const Buffer::Desc& bufferDesc );

		~Buffer();

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
			Buffer( device, allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true ) { }
	};
}