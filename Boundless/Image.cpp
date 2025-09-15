#include "Image.hpp"
#include "Buffer.hpp"


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

Boundless::Image::Image(
	const VkDevice& device,
	const VkPhysicalDevice& physicalDevice,
	uint32_t width,
	uint32_t height,
	uint32_t mipLevels,
	VkFormat format,
	VkImageTiling tiling,
	VkSampleCountFlagBits sampleCount,
	VkImageUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkImageCreateInfo* optionalCI
) : m_Device(device), m_Width(width), m_Height(height)
{
	VkImageCreateInfo imageInfo = optionalCI ? *optionalCI : Image::GetDefault2DCreateInfo(width, height, mipLevels, format, tiling, sampleCount, usage);
	if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS)
	{
		printf("Failed to create image\n");
	}

	VkMemoryRequirements memRequirements{};
	vkGetImageMemoryRequirements(m_Device, m_Image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = VkUtil::PhysicalDeviceFindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	if (const auto res = vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_ImageMemory); res != VK_SUCCESS)
	{
		printf("Failed to allocate image memory: %d\n", res);
	}

	vkBindImageMemory(m_Device, m_Image, m_ImageMemory, 0);
}

Boundless::Image::~Image()
{
}

VkImageCreateInfo Boundless::Image::GetDefault2DCreateInfo(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits sampleCount, VkImageUsageFlags usage) {
	VkImageCreateInfo res{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	res.imageType = VK_IMAGE_TYPE_2D;
	res.extent.width = width;
	res.extent.height = height;
	res.extent.depth = 1;
	res.mipLevels = mipLevels;
	res.arrayLayers = 1;
	res.format = format;
	res.tiling = tiling;
	res.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	res.usage = usage;
	res.samples = sampleCount;
	res.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return res;
}

Boundless::Image* Boundless::Image::LoadFromFile(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkCommandPool& commandPool, const VkQueue& graphicsQueue, const std::string& path)
{
	int width{}, height{}, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &texChannels, STBI_rgb_alpha);
	if (!pixels)
	{
		return nullptr;
	}

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

	VkDeviceSize imageSize = width * height * 4;
	std::unique_ptr<StagingBuffer> stagingBuffer = std::make_unique<StagingBuffer>(device, physicalDevice, imageSize);

	void* data = stagingBuffer->Map();
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	stagingBuffer->Unmap();

	stbi_image_free(pixels);

	VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

	Image* image = new Image(device, physicalDevice,
		width,
		height,
		mipLevels,
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	image->m_LevelCount = mipLevels;

	// Vulkan tutorial talks about using transition layout to go to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
	// But we can just set this for the VkImageView in the Descriptor.
	image->TransitionLayout(commandPool, graphicsQueue, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy image data from staging buffer to the VkImage.
	{
		VkCommandBuffer commandBuffer = VkUtil::CreateCommandBuffer(device, commandPool);

		VkUtil::CommandBufferBegin(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		VkUtil::CommandBufferCopyBufferToImage(commandBuffer, *stagingBuffer, *image, width, height);
		VkUtil::CommandBufferEnd(commandBuffer);
		VkUtil::CommandBufferSubmit(commandBuffer, graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	image->GenerateMipmaps(commandPool, graphicsQueue, width, height, mipLevels);
	image->m_Format = format;

	return image;
}

void Boundless::Image::TransitionLayout(const VkCommandPool& commandPool, const VkQueue& graphicsQueue, const VkFormat format, const VkImageLayout oldLayout, const VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = VkUtil::CreateCommandBuffer(m_Device, commandPool);

	VkUtil::CommandBufferBegin(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_Image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = m_LevelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage{}, destinationStage{};

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		printf("Attempted unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	VkUtil::CommandBufferEnd(commandBuffer);
	VkUtil::CommandBufferSubmit(commandBuffer, graphicsQueue);

	vkFreeCommandBuffers(m_Device, commandPool, 1, &commandBuffer);
}

void Boundless::Image::GenerateMipmaps(const VkCommandPool& commandPool, const VkQueue& graphicsQueue, uint32_t width, uint32_t height, uint32_t mipLevels)
{
	VkCommandBuffer commandBuffer = VkUtil::CreateCommandBuffer(m_Device, commandPool);

	VkUtil::CommandBufferBegin(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// Generate mipmaps.
	VkImageMemoryBarrier imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageBarrier.image = m_Image;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.layerCount = 1;
	imageBarrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = width;
	int32_t mipHeight = height;

	for (uint32_t i = 1u; i < mipLevels; i++) {
		imageBarrier.subresourceRange.baseMipLevel = i - 1;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr, 0, nullptr, 1, &imageBarrier);

		VkImageBlit imageBlit = {};
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.layerCount = 1;
		imageBlit.srcOffsets[0] = { 0, 0, 0 };
		imageBlit.srcOffsets[1] = { mipWidth, mipHeight, 1 };

		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.layerCount = 1;
		imageBlit.dstOffsets[0] = imageBlit.srcOffsets[0];
		imageBlit.dstOffsets[1] = { mipWidth > 1 ? (mipWidth >> 1) : 1 , mipHeight > 1 ? (mipHeight >> 1) : 1, 1 };

		//Blit the texture
		vkCmdBlitImage(commandBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &imageBlit, VK_FILTER_LINEAR);

		//Set the layout and Access Mask (again) for the shader to read
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		//Transfer the layout and Access Mask Information (Again, based on above values)
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		//Reduce the Mip level down by 1 level [By cutting width and height in half]
		if (mipWidth > 1) { mipWidth >>= 1; }
		if (mipHeight > 1) { mipHeight >>= 1; }
	}

	imageBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

	VkUtil::CommandBufferEnd(commandBuffer);
	VkUtil::CommandBufferSubmit(commandBuffer, graphicsQueue);

	vkFreeCommandBuffers(m_Device, commandPool, 1, &commandBuffer);
}