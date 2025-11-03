#include "Pch.hpp"
#include "Buffer.hpp"

namespace Boundless {
    Buffer::Buffer( const VkDevice& device, const VmaAllocator& allocator, const Buffer::Desc& bufferDesc) : m_Device( device ), m_Allocator( allocator ), m_Size( bufferDesc.m_Size ) {
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = bufferDesc.m_MemoryUsage;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if ( bufferDesc.m_Mappable )
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = bufferDesc.m_Size;
        bufferInfo.usage = bufferDesc.m_Usage;

        vmaCreateBuffer( allocator, &bufferInfo, &allocCreateInfo, &m_Handle, &m_Allocation, nullptr );
    }

    void Buffer::Release() { 
        if( m_Allocator && m_Handle && m_Allocation )
            vmaDestroyBuffer(m_Allocator, m_Handle, m_Allocation);
    }

    void* Buffer::Map() {
        void* data = nullptr;
        vmaMapMemory(m_Allocator, m_Allocation, &data);
        return data;
    }

    void Buffer::Unmap() {
        vmaUnmapMemory(m_Allocator, m_Allocation);
    }

    void Buffer::Patch(void* data, size_t size) {
        void* ptr = Map();
        memcpy(ptr, data, size);
        Unmap();
    }

    VkDeviceAddress Buffer::GetDeviceAddress() const {
        VkBufferDeviceAddressInfo deviceAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        deviceAddrInfo.buffer = GetHandle();
        return vkGetBufferDeviceAddress(m_Device, &deviceAddrInfo);
    }
}