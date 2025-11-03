#include "Pch.hpp"
#include "Device.hpp"

// TODO: Swap for differnet format possibly.
#include <ktxvulkan.h>
#pragma comment(lib, "ktx.lib")

namespace Boundless {
	Device::Device( HWND windowHandle ) { 
		uint32_t extensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions( &extensionCount );

		std::vector<const char*> extensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_RAY_QUERY_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
			// VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		};

		m_Instance = VkUtil::CreateInstance( glfwExtensions, extensionCount );

		volkLoadInstanceOnly( m_Instance );

		m_PhysicalDevice = VkUtil::GetPhysicalDevice( m_Instance, extensions );
		m_Surface	     = VkUtil::CreateSurfaceForWindow( m_Instance, windowHandle );
		m_Device		 = VkUtil::CreateLogicalDevice( m_Instance, m_PhysicalDevice, m_Surface, extensions );

		volkLoadDevice( m_Device );

		VmaAllocatorCreateInfo allocInfo = {};
		allocInfo.physicalDevice = m_PhysicalDevice;
		allocInfo.device = m_Device;
		allocInfo.instance = m_Instance;
		allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;
		allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		VmaVulkanFunctions functions = {};
		vmaImportVulkanFunctionsFromVolk( &allocInfo, &functions );

		allocInfo.pVulkanFunctions = &functions;
		vmaCreateAllocator( &allocInfo, &m_Allocator );

		const auto& queueFamilyIndices = VkUtil::FindQueueFamilyIndices( m_Surface, m_PhysicalDevice );
		m_GraphicsQueue = VkUtil::DeviceGetQueueByIndex( m_Device, queueFamilyIndices.m_GraphicsFamilyIndex );
		m_PresentQueue	= VkUtil::DeviceGetQueueByIndex( m_Device, queueFamilyIndices.m_PresentFamilyIndex );
		m_ComputeQueue	= VkUtil::DeviceGetQueueByIndex( m_Device, queueFamilyIndices.m_ComputeFamilyIndex );

		VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolCreateInfo.queueFamilyIndex = queueFamilyIndices.m_GraphicsFamilyIndex;

		m_CommandPool = VkUtil::CreateCommandPool( m_Device, poolCreateInfo );

		CreateGlobalDescriptors();
		CreateSamplers();
	}

	Device::~Device() { 
		vkDeviceWaitIdle(m_Device);

		ReleaseResources();

		vkDestroyDescriptorSetLayout( m_Device, m_TexturePoolLayout, nullptr );
		vkDestroyDescriptorPool( m_Device, m_DescriptorPool, nullptr );
		vkDestroyCommandPool( m_Device, m_CommandPool, nullptr );

		vmaDestroyAllocator( m_Allocator );

		vkDestroyDevice( m_Device, nullptr );
		vkDestroySurfaceKHR( m_Instance, m_Surface, nullptr );
		vkDestroyInstance( m_Instance, nullptr );
	}

	void Device::UploadImageToGPU( VkImageView imageView, uint32_t slotId ) { 
		VkDescriptorImageInfo imageInfo = { nullptr, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.dstBinding = 0;
		write.dstSet = m_TexturePool;
		write.descriptorCount = 1;
		write.dstArrayElement = slotId;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets( m_Device, 1, &write, 0, nullptr );
	}

	void Device::CreateSamplers() {
		m_Samplers[ LINEAR_CLAMP ] = CreateSampler( SamplerDesc{
				.m_MinFilter = VK_FILTER_LINEAR,
				.m_MagFilter = VK_FILTER_LINEAR,
				.m_WrapS = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_WrapT = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_Anisotropic = false
			} );

		m_Samplers[ ANISO_WRAP ] = CreateSampler( SamplerDesc {
				.m_MinFilter = VK_FILTER_LINEAR,
				.m_MagFilter = VK_FILTER_LINEAR,
				.m_WrapS = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.m_WrapT = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.m_Anisotropic = true
			} );
		
		m_Samplers[ ANISO_CLAMP ] = CreateSampler( SamplerDesc {
				.m_MinFilter = VK_FILTER_LINEAR,
				.m_MagFilter = VK_FILTER_LINEAR,
				.m_WrapS = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_WrapT = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.m_Anisotropic = true
			} );

		std::array<VkDescriptorImageInfo, SAMPLER_TYPE_MAX> samplerInfos = {};
		for ( uint32_t i = 0; i < SAMPLER_TYPE_MAX; i++ ) {
			samplerInfos[ i ].sampler = m_Samplers[ i ];
		}

		VkWriteDescriptorSet samplerWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		samplerWrite.dstSet = m_TexturePool;
		samplerWrite.dstBinding = 2;
		samplerWrite.descriptorCount = SAMPLER_TYPE_MAX;
		samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		samplerWrite.pImageInfo = samplerInfos.data();

		vkUpdateDescriptorSets( m_Device, 1, &samplerWrite, 0, nullptr );
	}

	void Device::CreateGlobalDescriptors() {
		VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024u, VK_SHADER_STAGE_ALL };
		VkDescriptorSetLayoutBinding accelerationStructureDescriptorSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1024u, VK_SHADER_STAGE_ALL };
		VkDescriptorSetLayoutBinding samplersDescriptorSetLayoutBinding = { 2, VK_DESCRIPTOR_TYPE_SAMPLER, SAMPLER_TYPE_MAX, VK_SHADER_STAGE_ALL };

		std::array<VkDescriptorBindingFlags, 3> descriptorBindingFlags = { VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
		std::array< VkDescriptorSetLayoutBinding, 3> bindings = { texturesDescriptorSetLayoutBinding, accelerationStructureDescriptorSetLayoutBinding, samplersDescriptorSetLayoutBinding };

		VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		descriptorSetLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();
		descriptorSetLayoutBindingFlags.bindingCount = uint32_t( descriptorBindingFlags.size() );

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlags;
		descriptorSetLayoutCreateInfo.bindingCount = uint32_t( bindings.size() );
		descriptorSetLayoutCreateInfo.pBindings = bindings.data();

		vkCreateDescriptorSetLayout( m_Device, &descriptorSetLayoutCreateInfo, nullptr, &m_TexturePoolLayout );

		VkDescriptorPoolSize descriptorPoolSizes[ ] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1024 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, SAMPLER_TYPE_MAX }
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType		   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.pNext		   = NULL;
		descriptorPoolCreateInfo.flags		   = 0;
		descriptorPoolCreateInfo.maxSets	   = 1;
		descriptorPoolCreateInfo.poolSizeCount = _countof( descriptorPoolSizes );
		descriptorPoolCreateInfo.pPoolSizes	   = descriptorPoolSizes;

		vkCreateDescriptorPool( m_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool );

		VkDescriptorSetAllocateInfo setAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		setAllocateInfo.descriptorPool = m_DescriptorPool;
		setAllocateInfo.pSetLayouts = &m_TexturePoolLayout;
		setAllocateInfo.descriptorSetCount = 1;
		vkAllocateDescriptorSets( m_Device, &setAllocateInfo, &m_TexturePool );
	}

	ImageHandle Device::CreateImage( const Image::Desc& imageDesc ) {
		Image res{};

		VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		createInfo.imageType     = imageDesc.m_Type;
		createInfo.extent.width  = imageDesc.m_Width;
		createInfo.extent.height = imageDesc.m_Height;
		createInfo.extent.depth  = 1;
		createInfo.mipLevels     = imageDesc.m_Levels;
		createInfo.arrayLayers   = imageDesc.m_Layers;
		createInfo.format		 = imageDesc.m_Format;
		createInfo.tiling		 = imageDesc.m_Tiling; // I have never not used optimal...
		createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		createInfo.usage		 = imageDesc.m_Usage;
		createInfo.samples		 = imageDesc.m_Samples;
		createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		vmaCreateImage( m_Allocator, &createInfo, &allocInfo, &res.m_Image, &res.m_Allocation, nullptr );

		size_t newId = m_AllImages.size();
		res.m_Resource = ImageHandle(newId);
		res.m_Desc	   = imageDesc;
		
		m_AllImages.push_back(res);
		
		return ImageHandle( newId );
	}

	ImageHandle Device::CreateImageView( ImageHandle resource, const Image::Desc& imageDesc ) {
		Image res = {};

		VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

		switch(imageDesc.m_Type) {
		case VK_IMAGE_TYPE_1D:
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
			break;
		case VK_IMAGE_TYPE_2D: 
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VK_IMAGE_TYPE_3D:
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		}

		createInfo.format = imageDesc.m_Format;

		bool isDepthImage = ( imageDesc.m_Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) || ( imageDesc.m_Format == VK_FORMAT_D32_SFLOAT );

		VkImageAspectFlags subresourceFlags = 
			isDepthImage ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		// TODO: Fixme.
		createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		createInfo.subresourceRange = { subresourceFlags, 0, imageDesc.m_Levels, 0, imageDesc.m_Layers };

		res.m_ImageView = VkUtil::CreateImageView( m_Device, m_AllImages[size_t(resource)], &createInfo );

		size_t newId = m_AllImages.size();

		if( imageDesc.m_Usage & VK_IMAGE_USAGE_SAMPLED_BIT ) {
			UploadImageToGPU( res.m_ImageView, uint32_t( newId ) );
		}

		m_AllImages.push_back( res );

		return ImageHandle(newId);
	}

	ImageHandle Device::CreateImageView( ImageHandle resource ) {
		return CreateImageView( resource, m_AllImages[ size_t( resource ) ].m_Desc );
	}

	BufferHandle Device::CreateBuffer( const Buffer::Desc& bufferDesc ) {
		size_t newId = m_AllBuffers.size();

		m_AllBuffers.emplace_back( m_Device, m_Allocator, bufferDesc );

		return static_cast< BufferHandle >( newId );
	}

	void Device::ReleaseResources() { 
		// Bugged.
		for ( Image& image : m_AllImages ) {
			vmaDestroyImage( m_Allocator, image.m_Image, image.m_Allocation );
		}

		m_AllImages.clear();
		m_AllBuffers.clear();
	}

	std::unique_ptr<StagingBuffer> Device::CreateStagingBuffer( const VkDeviceSize size ) {
		return std::make_unique<StagingBuffer>(m_Device, m_Allocator, size);
	}

	void Device::TransitionImageLayout( const VkImage& image, uint32_t levels, const VkFormat format, const VkImageLayout oldLayout, const VkImageLayout newLayout ) {
		VkCommandBuffer commandBuffer = CreateCommandBuffer();

		VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = levels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage{}, destinationStage{};

		if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else {
			printf( "Attempted unsupported layout transition!" );
		}

		vkCmdPipelineBarrier( commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );

		VkUtil::CommandBufferEnd( commandBuffer );
		VkUtil::CommandBufferSubmit( commandBuffer, m_GraphicsQueue );

		vkFreeCommandBuffers( m_Device, m_CommandPool, 1, &commandBuffer );
	}

	void Device::GenerateMipmaps( const VkImage& image, uint32_t width, uint32_t height, uint32_t mipLevels ) {
		VkCommandBuffer commandBuffer = CreateCommandBuffer( );

		VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

		// Generate mipmaps.
		VkImageMemoryBarrier imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageBarrier.image = image;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for ( uint32_t i = 1u; i < mipLevels; i++ ) {
			imageBarrier.subresourceRange.baseMipLevel = i - 1;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr, 0, nullptr, 1, &imageBarrier );

			VkImageBlit imageBlit = {};
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.baseArrayLayer = 0;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcOffsets[ 0 ] = { 0, 0, 0 };
			imageBlit.srcOffsets[ 1 ] = { mipWidth, mipHeight, 1 };

			imageBlit.dstSubresource.mipLevel = i;
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.baseArrayLayer = 0;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstOffsets[ 0 ] = imageBlit.srcOffsets[ 0 ];
			imageBlit.dstOffsets[ 1 ] = { mipWidth > 1 ? ( mipWidth >> 1 ) : 1 , mipHeight > 1 ? ( mipHeight >> 1 ) : 1, 1 };

			vkCmdBlitImage( commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &imageBlit, VK_FILTER_LINEAR );

			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &imageBarrier );

			if ( mipWidth > 1 ) { mipWidth >>= 1; }
			if ( mipHeight > 1 ) { mipHeight >>= 1; }
		}

		imageBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier );

		VkUtil::CommandBufferEnd( commandBuffer );
		VkUtil::CommandBufferSubmit( commandBuffer, m_GraphicsQueue );

		vkFreeCommandBuffers( m_Device, m_CommandPool, 1, &commandBuffer );
	}

	std::pair<ImageHandle, ImageHandle> Device::LoadImageFromFile( const std::string& path, bool isSRGB ) {
		int width{}, height{}, texChannels;
		stbi_uc* pixels = stbi_load( path.c_str(), &width, &height, &texChannels, STBI_rgb_alpha );
		if ( !pixels ) {
			return {};
		}

		uint32_t mipLevels = static_cast< uint32_t >( std::floor( std::log2( std::max( width, height ) ) ) ) + 1;

		VkDeviceSize imageSize = width * height * 4; // This is kinda ghetto.
		std::unique_ptr<StagingBuffer> stagingBuffer = std::make_unique<StagingBuffer>( m_Device, m_Allocator, imageSize );

		void* data = stagingBuffer->Map();
		memcpy( data, pixels, static_cast< size_t >( imageSize ) );
		stagingBuffer->Unmap();

		stbi_image_free( pixels );

		VkFormat format = isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

		ImageHandle handle = CreateImage(
			Image::Desc {
				.m_Type = VK_IMAGE_TYPE_2D,
				.m_Width = uint32_t(width),
				.m_Height = uint32_t(height),
				.m_Levels = mipLevels,
				.m_Format = format,
				.m_Tiling = VK_IMAGE_TILING_OPTIMAL,
				.m_Samples = VK_SAMPLE_COUNT_1_BIT,
				.m_Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			}
		);

		Image& image = GetImage(handle);

		TransitionImageLayout( image.m_Image, mipLevels, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

		{
			VkCommandBuffer commandBuffer = CreateCommandBuffer( );

			VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
			VkUtil::CommandBufferCopyBufferToImage( commandBuffer, *stagingBuffer, image.m_Image, width, height );
			VkUtil::CommandBufferEnd( commandBuffer );
			VkUtil::CommandBufferSubmit( commandBuffer, m_GraphicsQueue );

			vkFreeCommandBuffers( m_Device, m_CommandPool, 1, &commandBuffer );
		}

		GenerateMipmaps( image.m_Image, width, height, mipLevels );
		CreateImageView(handle);

		return { ImageHandle( m_AllImages.size() - 1 ), ImageHandle( m_AllImages.size() - 2) };
	}

	std::pair<ImageHandle, ImageHandle> Device::LoadKTXImageFromFile( const std::string& path ) {
		// This whole function should be redone.
		ktxVulkanDeviceInfo deviceInfo = {};

		ktx_error_code_e result = ktxVulkanDeviceInfo_Construct( &deviceInfo, m_PhysicalDevice, m_Device, m_GraphicsQueue, m_CommandPool, nullptr );
		if ( result != KTX_SUCCESS )
			return {};

		ktxTexture* kTexture = nullptr;
		result = ktxTexture_CreateFromNamedFile( path.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture );
		if ( result != KTX_SUCCESS )
			return {};

		ktxVulkanTexture texture = {};
		result = ktxTexture_VkUploadEx( kTexture, &deviceInfo, &texture, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if ( result != KTX_SUCCESS )
			return {};

		ktxTexture_Destroy( kTexture );
		ktxVulkanDeviceInfo_Destruct( &deviceInfo );

		Image res = {};
		res.m_Desc.m_Format = texture.imageFormat;
		res.m_Desc.m_Levels = texture.levelCount;
		res.m_Desc.m_Layers = texture.layerCount;
		res.m_Desc.m_Width = texture.width;
		res.m_Desc.m_Height = texture.height;
		res.m_Image = texture.image;
		
		m_AllImages.push_back( res );

		VkImageViewCreateInfo textureView = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		textureView.viewType = texture.viewType;
		textureView.format = texture.imageFormat;
		textureView.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		textureView.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levelCount, 0, texture.layerCount };
		
		res.m_ImageView = VkUtil::CreateImageView( m_Device, res.m_Image, &textureView );
		m_AllImages.push_back( res );

		ImageHandle viewHandle = ImageHandle( m_AllImages.size() - 1 );
		ImageHandle imageHandle = ImageHandle( m_AllImages.size() - 2 );

		UploadImageToGPU( res.m_ImageView, uint32_t(viewHandle) );
		
		return { viewHandle, imageHandle };
	}

	VkCommandBuffer Device::CreateCommandBuffer() {
		return VkUtil::CreateCommandBuffer( m_Device, m_CommandPool );
	}

	VkSampler Device::CreateSampler( const SamplerDesc& samplerDesc ) {
		VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.flags = 0;
		samplerInfo.addressModeU = samplerDesc.m_WrapS;
		samplerInfo.addressModeV = samplerDesc.m_WrapT;
		samplerInfo.minFilter = samplerDesc.m_MinFilter;
		samplerInfo.magFilter = samplerDesc.m_MagFilter;
		samplerInfo.mipmapMode = ( samplerDesc.m_MinFilter == VK_FILTER_NEAREST || samplerDesc.m_MagFilter == VK_FILTER_NEAREST ) ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0;
		samplerInfo.minLod = 0.f;
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerInfo.anisotropyEnable = samplerDesc.m_Anisotropic;
		samplerInfo.maxAnisotropy = 16.f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerInfo.compareEnable = VK_TRUE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.pNext = nullptr;

		VkSampler sampler = VK_NULL_HANDLE;
		vkCreateSampler( m_Device, &samplerInfo, nullptr, &sampler );

		return sampler;
	}
}