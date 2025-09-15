#include "Buffer.hpp"

Boundless::Buffer::Buffer(
    const VkDevice& device,
    const VkPhysicalDevice& physicalDevice,
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags memoryFlags,
    const VkSharingMode sharingMode,
    const VkMemoryAllocateFlags allocateFlags) : m_Device(device), m_PhysicalDevice(physicalDevice), m_Size(size) {
    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = sharingMode;

    if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &m_Handle) != VK_SUCCESS) {
        printf("Failed to create buffer!\n");
        return;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_Device, m_Handle, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = VkUtil::PhysicalDeviceFindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, memoryFlags);

    VkMemoryAllocateFlagsInfo flagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };

    if (allocateFlags != 0) {
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        memoryAllocInfo.pNext = &flagsInfo;
    }

    if (vkAllocateMemory(m_Device, &memoryAllocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
        printf("failed to allocate vertex buffer memory!\n");
        return;
    }

    vkBindBufferMemory(m_Device, m_Handle, m_Memory, 0);
}

Boundless::Buffer::Buffer(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const Buffer::Desc& bufferDesc) : 
    Buffer(device, physicalDevice, bufferDesc.m_Size, bufferDesc.m_Usage, bufferDesc.m_MemoryFlags, bufferDesc.m_SharingMode, bufferDesc.m_AllocateFlags) {}

Boundless::Buffer::~Buffer() {
    vkDestroyBuffer(m_Device, m_Handle, nullptr);
    vkFreeMemory(m_Device, m_Memory, nullptr);

    m_Handle = VK_NULL_HANDLE;
    m_Memory = VK_NULL_HANDLE;
}

void* Boundless::Buffer::Map() {
    void* data = nullptr;
    vkMapMemory(m_Device, m_Memory, 0, VK_WHOLE_SIZE, 0, &data);
    return data;
}

void Boundless::Buffer::Unmap() {
    vkUnmapMemory(m_Device, m_Memory);
}

void Boundless::Buffer::Patch(void* data, size_t size) {
    void* ptr = Map();
    memcpy(ptr, data, size);
    Unmap();
}

const VkDeviceAddress& Boundless::Buffer::GetDeviceAddress() const {
    VkBufferDeviceAddressInfo deviceAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    deviceAddrInfo.buffer = GetHandle();
    return vkGetBufferDeviceAddress(m_Device, &deviceAddrInfo);
}