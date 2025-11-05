#include "Pch.hpp"
#include "Buffer.hpp"

namespace Boundless {
    Buffer::Buffer( const vk::Device& device, const VmaAllocator& allocator, const Buffer::Desc& bufferDesc) : m_Device( device ), m_Allocator( allocator ), m_Size( bufferDesc.m_Size ) {
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = bufferDesc.m_MemoryUsage;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if ( bufferDesc.m_Mappable )
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = VkDeviceSize( bufferDesc.m_Size );
        bufferInfo.usage = VkBufferUsageFlags( bufferDesc.m_Usage );

        VkBuffer handle = VK_NULL_HANDLE;
        vmaCreateBuffer( allocator, &bufferInfo, &allocCreateInfo, &handle, &m_Allocation, nullptr );
        
        m_Handle = vk::Buffer(handle);
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

    vk::DeviceAddress Buffer::GetDeviceAddress() const {
        return m_Device.getBufferAddress( vk::BufferDeviceAddressInfo{ m_Handle } );
    }
}