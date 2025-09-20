#include "Buffer.hpp"

namespace Boundless {
    Buffer::Buffer(
        const VkDevice& device,
        const VmaAllocator& allocator,
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VmaMemoryUsage memoryUsage,
        bool mappable ) : m_Device(device), m_Allocator(allocator), m_Size(size) {

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = memoryUsage;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        
        if(mappable)
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        
        vmaCreateBuffer( allocator, &bufferInfo, &allocCreateInfo, &m_Handle, &m_Allocation, nullptr );
    }

    Buffer::Buffer( const VkDevice& device, const VmaAllocator& allocator, const Buffer::Desc& bufferDesc) :
        Buffer(device, allocator, bufferDesc.m_Size, bufferDesc.m_Usage, bufferDesc.m_MemoryUsage, bufferDesc.m_Mappable) {}

    Buffer::~Buffer() {
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

    const VkDeviceAddress& Buffer::GetDeviceAddress() const {
        VkBufferDeviceAddressInfo deviceAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        deviceAddrInfo.buffer = GetHandle();
        return vkGetBufferDeviceAddress(m_Device, &deviceAddrInfo);
    }
}