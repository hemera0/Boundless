#include "Pch.hpp"
#include "Device.hpp"
#include "Pipelines.hpp"
#include "CommandBuffer.hpp"

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

		m_Instance = vk_util::CreateInstance( glfwExtensions, extensionCount );
		VULKAN_HPP_DEFAULT_DISPATCHER.init( m_Instance );

		m_PhysicalDevice = vk_util::GetPhysicalDevice( m_Instance, extensions );
		m_Surface	     = vk_util::CreateSurfaceForWindow( m_Instance, windowHandle );
		m_Device		 = vk_util::CreateLogicalDevice( m_Instance, m_PhysicalDevice, m_Surface, extensions );
		VULKAN_HPP_DEFAULT_DISPATCHER.init( m_Device );

		VmaAllocatorCreateInfo allocInfo = {};
		allocInfo.physicalDevice = m_PhysicalDevice;
		allocInfo.device = m_Device;
		allocInfo.instance = m_Instance;
		allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;
		allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		static vk::detail::DynamicLoader vmadl;

		VmaVulkanFunctions functions = {};
		functions.vkGetInstanceProcAddr = vmadl.getProcAddress<decltype(functions.vkGetInstanceProcAddr)>( "vkGetInstanceProcAddr" );
		functions.vkGetDeviceProcAddr = vmadl.getProcAddress<decltype(functions.vkGetDeviceProcAddr)>( "vkGetDeviceProcAddr" );
		allocInfo.pVulkanFunctions = &functions;

		vmaCreateAllocator( &allocInfo, &m_Allocator );

		m_QueueIndex = vk_util::FindQueueFamilyIndex( m_Surface, m_PhysicalDevice );
		m_Queue = m_Device.getQueue( m_QueueIndex, 0 );
		
		// TODO: maybe move or remove this? (It's useful though for things like mipmap gen, staging buffer copies, etc.)
		m_CommandPool = m_Device.createCommandPool( vk::CommandPoolCreateInfo( vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_QueueIndex ) );

		CreateGlobalDescriptors();
		CreateSamplers();
	}

	Device::~Device() { 
		m_Device.waitIdle();
		m_Device.destroyDescriptorSetLayout( m_GlobalResourceLayout );
		m_Device.destroyDescriptorPool( m_DescriptorPool );
		m_Device.destroyCommandPool( m_CommandPool );

		vmaDestroyAllocator( m_Allocator );

		m_Device.destroy();
		m_Instance.destroySurfaceKHR( m_Surface );
		m_Instance.destroy( );
	}

	void Device::UploadImageToGPU( const vk::ImageView& imageView, uint32_t slotId ) {
		vk::DescriptorImageInfo imageInfo = { nullptr, imageView, vk::ImageLayout::eShaderReadOnlyOptimal };
		vk::WriteDescriptorSet      write = { m_GlobalResources, 0, slotId, vk::DescriptorType::eSampledImage, imageInfo };

		m_Device.updateDescriptorSets( { write }, nullptr );
	}

	void Device::CreateSamplers() {
		m_Samplers[ LINEAR_CLAMP ] = CreateSampler( SamplerDesc{
				.m_MinFilter = vk::Filter::eLinear,
				.m_MagFilter = vk::Filter::eLinear,
				.m_WrapS = vk::SamplerAddressMode::eClampToEdge,
				.m_WrapT = vk::SamplerAddressMode::eClampToEdge,
				.m_Anisotropic = false
			} );

		m_Samplers[ ANISO_WRAP ] = CreateSampler( SamplerDesc {
				.m_MinFilter = vk::Filter::eLinear,
				.m_MagFilter = vk::Filter::eLinear,
				.m_WrapS = vk::SamplerAddressMode::eRepeat,
				.m_WrapT = vk::SamplerAddressMode::eRepeat,
				.m_Anisotropic = true
			} );
		
		m_Samplers[ ANISO_CLAMP ] = CreateSampler( SamplerDesc {
				.m_MinFilter = vk::Filter::eLinear,
				.m_MagFilter = vk::Filter::eLinear,
				.m_WrapS = vk::SamplerAddressMode::eClampToEdge,
				.m_WrapT = vk::SamplerAddressMode::eClampToEdge,
				.m_Anisotropic = true
			} );

		std::array<vk::DescriptorImageInfo, SAMPLER_TYPE_MAX> samplerInfos = {};
		for ( uint32_t i = 0; i < SAMPLER_TYPE_MAX; i++ ) {
			samplerInfos[ i ].sampler = m_Samplers[ i ];
		}
		
		vk::WriteDescriptorSet write = { m_GlobalResources, 2, 0, vk::DescriptorType::eSampler, samplerInfos };
		m_Device.updateDescriptorSets( { write }, nullptr );
	}

	void Device::CreateGlobalDescriptors() {
		vk::DescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = { 0, vk::DescriptorType::eSampledImage, 1024u, vk::ShaderStageFlagBits::eAll };
		vk::DescriptorSetLayoutBinding accelerationStructureDescriptorSetLayoutBinding = { 1, vk::DescriptorType::eAccelerationStructureKHR, 1024u, vk::ShaderStageFlagBits::eAll };
		vk::DescriptorSetLayoutBinding samplersDescriptorSetLayoutBinding = { 2, vk::DescriptorType::eSampler, SAMPLER_TYPE_MAX, vk::ShaderStageFlagBits::eAll };

		std::array<vk::DescriptorBindingFlags, 3> descriptorBindingFlags = { vk::DescriptorBindingFlagBits::ePartiallyBound, vk::DescriptorBindingFlagBits::ePartiallyBound, vk::DescriptorBindingFlagBits::ePartiallyBound };
		std::array<vk::DescriptorSetLayoutBinding, 3> bindings = { texturesDescriptorSetLayoutBinding, accelerationStructureDescriptorSetLayoutBinding, samplersDescriptorSetLayoutBinding };

		vk::DescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags = {};
		descriptorSetLayoutBindingFlags.setBindingFlags( descriptorBindingFlags );

		vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.setBindings(bindings);
		descriptorSetLayoutCreateInfo.setPNext( &descriptorSetLayoutBindingFlags );

		m_GlobalResourceLayout = m_Device.createDescriptorSetLayout( descriptorSetLayoutCreateInfo );

		std::array<vk::DescriptorPoolSize, 3> descriptorPoolSizes = {
			vk::DescriptorPoolSize{ vk::DescriptorType::eSampledImage, 1024u },
			vk::DescriptorPoolSize{ vk::DescriptorType::eAccelerationStructureKHR, 1024u },
			vk::DescriptorPoolSize{ vk::DescriptorType::eSampler, SAMPLER_TYPE_MAX }
		};

		vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.setMaxSets( 1 );
		descriptorPoolCreateInfo.setPoolSizes( descriptorPoolSizes );

		m_DescriptorPool = m_Device.createDescriptorPool( descriptorPoolCreateInfo );

		vk::DescriptorSetAllocateInfo setAllocateInfo = {};
		setAllocateInfo.setDescriptorPool( m_DescriptorPool )
			.setSetLayouts( { m_GlobalResourceLayout } );

		m_GlobalResources = m_Device.allocateDescriptorSets( setAllocateInfo )[ 0 ];

		m_GlobalPipelineLayout = PipelineLayoutBuilder()
			.SetPushConstants( { vk::PushConstantRange{ vk::ShaderStageFlagBits::eAll, 0, 128u } } )
			.SetDescriptorSets( { m_GlobalResourceLayout } )
			.Build( *this );
	}

	ImageHandle Device::CreateImage( const Image::Desc& imageDesc ) {
		Image res = {};

		vk::ImageCreateInfo createInfo = {};
		createInfo.setImageType( imageDesc.m_Type )
			.setExtent( { imageDesc.m_Width, imageDesc.m_Height, 1 } )
			.setMipLevels( imageDesc.m_Levels )
			.setArrayLayers( imageDesc.m_Layers )
			.setFormat( imageDesc.m_Format )
			.setTiling( imageDesc.m_Tiling ) 
			.setInitialLayout( vk::ImageLayout::eUndefined )
			.setUsage( imageDesc.m_Usage )
			.setSamples( imageDesc.m_Samples )
			.setSharingMode( vk::SharingMode::eExclusive );

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		VkImageCreateInfo oldCreateInfo = createInfo;
		VkImage imageHandle = VK_NULL_HANDLE;
		vmaCreateImage( m_Allocator, &oldCreateInfo, &allocInfo, &imageHandle, &res.m_Allocation, nullptr );

		size_t newId = m_AllImages.size();
		res.m_Image    = vk::Image( imageHandle );
		res.m_Resource = ImageHandle( newId );
		res.m_Desc	   = imageDesc;
		m_AllImages.push_back(res);
		
		return res.m_Resource;
	}

	ImageHandle Device::CreateImageView( ImageHandle resource, const Image::Desc& imageDesc ) {
		Image res = {};

		vk::ImageViewCreateInfo createInfo = {};

		switch(imageDesc.m_Type) {
		case vk::ImageType::e1D:
			createInfo.viewType = vk::ImageViewType::e1D;
			break;
		case vk::ImageType::e2D:
			createInfo.viewType = vk::ImageViewType::e2D;
			break;
		case vk::ImageType::e3D:
			createInfo.viewType = vk::ImageViewType::e3D;
			break;
		}

		createInfo.format = imageDesc.m_Format;
		createInfo.image = m_AllImages[ size_t( resource ) ];

		bool isDepthImage = ( imageDesc.m_Usage & vk::ImageUsageFlagBits::eDepthStencilAttachment ) || ( imageDesc.m_Format == vk::Format::eD32Sfloat );
		vk::ImageAspectFlags aspectMask =  isDepthImage ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

		// TODO: Fixme. (I think it's good?)
		createInfo.setSubresourceRange( vk::ImageSubresourceRange{ aspectMask, 0, imageDesc.m_Levels, 0, imageDesc.m_Layers });

		res.m_ImageView = m_Device.createImageView( createInfo );

		size_t newId = m_AllImages.size();

		if( imageDesc.m_Usage & vk::ImageUsageFlagBits::eSampled ) {
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
		return BufferHandle( newId );
	}

	std::unique_ptr<StagingBuffer> Device::CreateStagingBuffer( const vk::DeviceSize size ) {
		return std::make_unique<StagingBuffer>(m_Device, m_Allocator, size);
	}

	void Device::TransitionImageLayout( const vk::Image& image, uint32_t levels, const vk::Format format, const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout ) {
		CommandBuffer commandBuffer = CommandBuffer(*this);
		commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );

		vk::ImageMemoryBarrier barrier{};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, levels, 0, vk::RemainingArrayLayers };

		vk::PipelineStageFlags sourceStage{}, destinationStage{};
		if ( oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal ) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			
			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		} else if ( oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal ) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		} else {
			printf( "Attempted unsupported layout transition!" );
		}

		commandBuffer->pipelineBarrier( sourceStage, destinationStage, {}, {}, {}, { barrier } );
		commandBuffer.End();
		commandBuffer.Submit(m_Queue);
	}

	void Device::GenerateMipmaps( const vk::Image& image, uint32_t width, uint32_t height, uint32_t mipLevels ) {
		CommandBuffer commandBuffer = CommandBuffer( *this );
		commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );

		// Generate mipmaps.
		vk::ImageMemoryBarrier imageBarrier = {};
		imageBarrier.image = image;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = width;
		int32_t mipHeight = height;

		for ( uint32_t i = 1u; i < mipLevels; i++ ) {
			imageBarrier.subresourceRange.baseMipLevel = i - 1;
			imageBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			imageBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			imageBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			imageBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			commandBuffer->pipelineBarrier( vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, { imageBarrier } );

			vk::ImageBlit imageBlit = {};
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			imageBlit.srcSubresource.baseArrayLayer = 0;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcOffsets[ 0 ] = vk::Offset3D{ 0, 0, 0 };
			imageBlit.srcOffsets[ 1 ] = vk::Offset3D{ mipWidth, mipHeight, 1 };

			imageBlit.dstSubresource.mipLevel = i;
			imageBlit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			imageBlit.dstSubresource.baseArrayLayer = 0;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstOffsets[ 0 ] = imageBlit.srcOffsets[ 0 ];
			imageBlit.dstOffsets[ 1 ] = vk::Offset3D{ mipWidth > 1 ? ( mipWidth >> 1 ) : 1 , mipHeight > 1 ? ( mipHeight >> 1 ) : 1, 1 };

			commandBuffer->blitImage( image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, { imageBlit }, vk::Filter::eLinear );

			imageBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			imageBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			imageBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			imageBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			commandBuffer->pipelineBarrier( vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, { imageBarrier } );

			if ( mipWidth > 1 ) { mipWidth >>= 1; }
			if ( mipHeight > 1 ) { mipHeight >>= 1; }
		}

		imageBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
		imageBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		imageBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		imageBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer->pipelineBarrier( vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, { imageBarrier } );

		commandBuffer.End();
		commandBuffer.Submit( m_Queue );
	}

	std::pair<ImageHandle, ImageHandle> Device::LoadImageFromFile( const std::string& path, bool isSRGB ) {
		int width{}, height{}, texChannels;
		stbi_uc* pixels = stbi_load( path.c_str(), &width, &height, &texChannels, STBI_rgb_alpha );
		if ( !pixels ) {
			return {};
		}

		uint32_t mipLevels = uint32_t( std::floor( std::log2( std::max( width, height ) ) ) ) + 1;

		vk::DeviceSize imageSize = width * height * 4; // This is kinda ghetto.
		std::unique_ptr<StagingBuffer> stagingBuffer = std::make_unique<StagingBuffer>( m_Device, m_Allocator, imageSize );

		void* data = stagingBuffer->Map();
		memcpy( data, pixels, size_t( imageSize ) );
		stagingBuffer->Unmap();

		stbi_image_free( pixels );

		vk::Format format = isSRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

		ImageHandle handle = CreateImage(
			Image::Desc {
				.m_Type = vk::ImageType::e2D,
				.m_Width = uint32_t(width),
				.m_Height = uint32_t(height),
				.m_Levels = mipLevels,
				.m_Format = format,
				.m_Tiling = vk::ImageTiling::eOptimal,
				.m_Samples = vk::SampleCountFlagBits::e1,
				.m_Usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled
			}
		);

		Image& image = GetImage(handle);

		TransitionImageLayout( image.m_Image, mipLevels, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal );

		{
			CommandBuffer commandBuffer = CommandBuffer( *this );
			commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
			commandBuffer.CopyBufferToImage( *stagingBuffer, image.m_Image, width, height );
			commandBuffer.End();
			commandBuffer.Submit( m_Queue );
		}

		GenerateMipmaps( image.m_Image, width, height, mipLevels );
		CreateImageView( handle );

		return { ImageHandle( m_AllImages.size() - 1 ), ImageHandle( m_AllImages.size() - 2) };
	}

	std::pair<ImageHandle, ImageHandle> Device::LoadKTXImageFromFile( const std::string& path ) {
		// This whole function should be redone.
		ktxVulkanDeviceInfo deviceInfo = {};

		ktx_error_code_e result = ktxVulkanDeviceInfo_Construct( &deviceInfo, m_PhysicalDevice, m_Device, m_Queue, m_CommandPool, nullptr );
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
		res.m_Desc.m_Format = vk::Format( texture.imageFormat );
		res.m_Desc.m_Levels = texture.levelCount;
		res.m_Desc.m_Layers = texture.layerCount;
		res.m_Desc.m_Width = texture.width;
		res.m_Desc.m_Height = texture.height;
		res.m_Image = vk::Image( texture.image );
		
		m_AllImages.push_back( res );

		vk::ImageViewCreateInfo textureView = {};
		textureView.viewType = vk::ImageViewType( texture.viewType );
		textureView.format = vk::Format( texture.imageFormat );
		textureView.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, texture.levelCount, 0, texture.layerCount };
		textureView.image = res.m_Image;

		res.m_ImageView = m_Device.createImageView( textureView );
		m_AllImages.push_back( res );

		ImageHandle viewHandle = ImageHandle( m_AllImages.size() - 1 );
		ImageHandle imageHandle = ImageHandle( m_AllImages.size() - 2 );

		UploadImageToGPU( res.m_ImageView, uint32_t(viewHandle) );
		
		return { viewHandle, imageHandle };
	}

	vk::Sampler Device::CreateSampler( const SamplerDesc& samplerDesc ) {
		vk::SamplerCreateInfo samplerInfo = {};
		samplerInfo.addressModeU = samplerDesc.m_WrapS;
		samplerInfo.addressModeV = samplerDesc.m_WrapT;
		samplerInfo.minFilter = samplerDesc.m_MinFilter;
		samplerInfo.magFilter = samplerDesc.m_MagFilter;
		samplerInfo.mipmapMode = ( samplerDesc.m_MinFilter == vk::Filter::eNearest || samplerDesc.m_MagFilter == vk::Filter::eNearest ) ? vk::SamplerMipmapMode::eNearest : vk::SamplerMipmapMode::eLinear;
		samplerInfo.mipLodBias = 0;
		samplerInfo.minLod = 0.f;
		samplerInfo.maxLod = vk::LodClampNone;
		samplerInfo.anisotropyEnable = samplerDesc.m_Anisotropic;
		samplerInfo.maxAnisotropy = 16.f;
		samplerInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
		samplerInfo.compareEnable = vk::True;
		samplerInfo.compareOp = vk::CompareOp::eAlways;
		samplerInfo.pNext = nullptr;
		return m_Device.createSampler( samplerInfo );;
	}

	void Device::ReleaseImage( ImageHandle handle ) { 
		m_Device.waitIdle();
		
		// TODO: free index.
		Image& image = m_AllImages[ size_t( handle ) ];
		vmaDestroyImage(m_Allocator, image.m_Image, image.m_Allocation);
	}
	
	void Device::ReleaseImageView( ImageHandle handle ) { 
		m_Device.waitIdle();

		// TODO: free index.
		Image& image = m_AllImages[ size_t( handle ) ];
		m_Device.destroyImageView( image.m_ImageView );
	}
}