#pragma once
#include "VkUtil.hpp"

// TODO: switch to VMA...
// Represents a Vulkan buffer handle and it's memory...

namespace Boundless {
	class Buffer {
		VkDevice m_Device = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

		VkBuffer m_Handle = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;

		VkDeviceSize m_Size{};
	public:
		struct Desc {
			VkDeviceSize m_Size{};
			VkBufferUsageFlags m_Usage{};
			VkMemoryPropertyFlags m_MemoryFlags{};
			VkSharingMode m_SharingMode{};
			VkMemoryAllocateFlags m_AllocateFlags{};
		};

		Buffer( const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags memoryFlags, const VkSharingMode sharingMode, const VkMemoryAllocateFlags allocateFlags = 0 );
		Buffer( const VkDevice& device, const VkPhysicalDevice& physicalDevice, const Buffer::Desc& bufferDesc );

		~Buffer();

		void* Map();
		void Unmap();

		void Patch( void* data, size_t size );

		const VkDeviceMemory& GetMemory() const { return m_Memory; }
		const VkBuffer& GetHandle() const { return m_Handle; }
		const VkDeviceSize GetSize() const { return m_Size; }
		const VkDeviceAddress& GetDeviceAddress() const;

		operator VkBuffer() const {
			return m_Handle;
		}
	};

	class StagingBuffer : public Buffer {
	public:
		StagingBuffer( const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkDeviceSize size ) :
			Buffer( device, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE ) { }
	};
}