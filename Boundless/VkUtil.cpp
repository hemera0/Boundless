#include "Pch.hpp"
#include "VkUtil.hpp"

namespace VkUtil {
	constexpr const bool UseValidationLayers = true;

	vk::Instance CreateInstance( const char** requiredExtensions, const int requiredExtensionsCount ) {
		vk::ApplicationInfo appInfo = {};
		appInfo.setPEngineName( "Boundless" )
			.setEngineVersion( VK_MAKE_VERSION( 1, 0, 0 ) )
			.setApiVersion( VK_API_VERSION_1_4 );

		vk::InstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.setPApplicationInfo( &appInfo );

		if ( requiredExtensions && requiredExtensionsCount ) {
			instanceCreateInfo.setEnabledExtensionCount( requiredExtensionsCount )
				.setPpEnabledExtensionNames( requiredExtensions );
		}

		if constexpr ( UseValidationLayers ) {
			std::array<const char*, 1> validationLayers = {
				"VK_LAYER_KHRONOS_validation",
			};

			std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties( );

			size_t supportedLayersCount = 0;

			for ( const auto& wantedLayer : validationLayers ) {
				for ( const auto& availableLayer : availableLayers ) {
					if ( strcmp( wantedLayer, availableLayer.layerName ) == 0 )
						supportedLayersCount++;
				}
			}

			vk::ValidationFeaturesEXT extValidationFeatures = {};
			vk::ValidationFeatureEnableEXT enableShaderPrintf = vk::ValidationFeatureEnableEXT::eDebugPrintf;
			vk::ValidationFeatureDisableEXT disableUniqueHandles = vk::ValidationFeatureDisableEXT::eUniqueHandles;

			extValidationFeatures.setEnabledValidationFeatureCount( 1 )
				.setPEnabledValidationFeatures( &enableShaderPrintf )
				.setDisabledValidationFeatureCount( 1 )
				.setPDisabledValidationFeatures( &disableUniqueHandles );

			instanceCreateInfo.setPEnabledLayerNames( validationLayers )
				.setPNext( &extValidationFeatures );
		}

		return vk::createInstance( instanceCreateInfo, nullptr );
	}

	vk::PhysicalDevice GetPhysicalDevice( const vk::Instance& instance, const std::vector<const char*>& wantedExtensions ) {
		std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices(  );
		if ( devices.empty() ) {
			printf( "Failed to find vulkan enabled GPUs\n" );
			return VK_NULL_HANDLE;
		}

		for ( const vk::PhysicalDevice& device : devices ) {
			vk::PhysicalDeviceProperties deviceProps = device.getProperties();

			if ( deviceProps.deviceType != vk::PhysicalDeviceType::eDiscreteGpu || !PhysicalDeviceHasExtensions( device, wantedExtensions ) )
				continue;

			printf( "Using %s\n", deviceProps.deviceName.data() );

			return device;
		}

		return VK_NULL_HANDLE;
	}

	vk::Device CreateLogicalDevice( const vk::Instance& instance, const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const std::vector<const char*>& wantedExtensions ) {
		const auto& swapChainSupport = QuerySwapchainSupport( surface, physicalDevice );
		if ( swapChainSupport.m_PresentModes.empty() || swapChainSupport.m_SurfaceFormats.empty() ) {
			printf( "Device does not have any present modes or surface formats\n" );
			return VK_NULL_HANDLE;
		}

		uint32_t queueFamilyIndex = FindQueueFamilyIndex( surface, physicalDevice );
		if ( queueFamilyIndex == vk::QueueFamilyIgnored ) {
			printf( "Failed to find valid queue family\n" );
			return VK_NULL_HANDLE;
		}

		float queuePriority = 1.0f;

		vk::StructureChain queryChain = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan14Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan12Features>();
		
		vk::PhysicalDeviceVulkan14Features& testFeatures14 = queryChain.get<vk::PhysicalDeviceVulkan14Features>();
		vk::PhysicalDeviceVulkan13Features& testFeatures13 = queryChain.get<vk::PhysicalDeviceVulkan13Features>();
		vk::PhysicalDeviceVulkan12Features& testFeatures12 = queryChain.get<vk::PhysicalDeviceVulkan12Features>();
		vk::PhysicalDeviceFeatures2&		  testFeatures = queryChain.get<vk::PhysicalDeviceFeatures2>();

		 vk::PhysicalDeviceFeatures deviceFeatures{};
		 if ( testFeatures.features.samplerAnisotropy )
		 	deviceFeatures.samplerAnisotropy = vk::True;
		 if ( testFeatures.features.multiDrawIndirect )
		 	deviceFeatures.multiDrawIndirect = vk::True;
		 if ( testFeatures.features.depthClamp )
		 	deviceFeatures.depthClamp = vk::True;
		 if ( testFeatures.features.drawIndirectFirstInstance )
		 	deviceFeatures.drawIndirectFirstInstance = vk::True;
		 
		 // TODO: Verify this is correct?
		 if ( testFeatures12.shaderFloat16 )
		 	deviceFeatures.shaderInt16 = vk::True;

		vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceVulkan14Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan12Features,
			vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceRayQueryFeaturesKHR, vk::PhysicalDeviceAccelerationStructureFeaturesKHR> featuresChain = {};

		vk::DeviceCreateInfo& deviceCreateInfo = featuresChain.get<vk::DeviceCreateInfo>();
		deviceCreateInfo.setPEnabledFeatures(&deviceFeatures);
		deviceCreateInfo.setPEnabledExtensionNames( wantedExtensions );

		vk::DeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.setQueueFamilyIndex( queueFamilyIndex )
			.setQueueCount( 1 )
			.setPQueuePriorities( &queuePriority );

		deviceCreateInfo.setQueueCreateInfos( queueCreateInfo );
		deviceCreateInfo.setEnabledLayerCount( 0 );

	#define LOG_FEATURE(x) printf("\t" #x ": %s\n", x ? "true" : "false");

		vk::PhysicalDeviceVulkan14Features& features14 = featuresChain.get<vk::PhysicalDeviceVulkan14Features>();
		vk::PhysicalDeviceVulkan13Features& features13 = featuresChain.get<vk::PhysicalDeviceVulkan13Features>();
		vk::PhysicalDeviceVulkan12Features& features12 = featuresChain.get<vk::PhysicalDeviceVulkan12Features>();
		vk::PhysicalDeviceVulkan11Features& features11 = featuresChain.get<vk::PhysicalDeviceVulkan11Features>();
		vk::PhysicalDeviceRayQueryFeaturesKHR& featuresRayQuery = featuresChain.get<vk::PhysicalDeviceRayQueryFeaturesKHR>();
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR& featuresAccelerationStructure = featuresChain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();

		// Implement 1.4 features.
		features14.maintenance5 = testFeatures14.maintenance5;
		LOG_FEATURE( features14.maintenance5 );

		// Implement 1.3 features.
		features13.dynamicRendering = testFeatures13.dynamicRendering;
		features13.synchronization2 = testFeatures13.synchronization2;
		LOG_FEATURE( features13.dynamicRendering );
		LOG_FEATURE( features13.synchronization2 );

		// Implement 1.2 features.
		features12.shaderFloat16							  = testFeatures12.shaderFloat16;
		features12.descriptorIndexing						  = testFeatures12.descriptorIndexing;
		features12.shaderSampledImageArrayNonUniformIndexing  = testFeatures12.shaderSampledImageArrayNonUniformIndexing;
		features12.shaderStorageBufferArrayNonUniformIndexing = testFeatures12.shaderStorageBufferArrayNonUniformIndexing;
		features12.shaderUniformBufferArrayNonUniformIndexing = testFeatures12.shaderUniformBufferArrayNonUniformIndexing;
		features12.runtimeDescriptorArray					  = testFeatures12.runtimeDescriptorArray;
		features12.descriptorBindingVariableDescriptorCount	  = testFeatures12.descriptorBindingVariableDescriptorCount;
		features12.descriptorBindingPartiallyBound			  = testFeatures12.descriptorBindingPartiallyBound;
		features12.bufferDeviceAddress						  = testFeatures12.bufferDeviceAddress;

		LOG_FEATURE( features12.shaderFloat16 );
		LOG_FEATURE( features12.descriptorIndexing );
		LOG_FEATURE( features12.shaderSampledImageArrayNonUniformIndexing );
		LOG_FEATURE( features12.shaderStorageBufferArrayNonUniformIndexing );
		LOG_FEATURE( features12.shaderUniformBufferArrayNonUniformIndexing );
		LOG_FEATURE( features12.runtimeDescriptorArray );
		LOG_FEATURE( features12.descriptorBindingVariableDescriptorCount );
		LOG_FEATURE( features12.descriptorBindingPartiallyBound );
		LOG_FEATURE( features12.bufferDeviceAddress );

		// Implement 1.1 features.
		features11.shaderDrawParameters = true;
		LOG_FEATURE( features11.shaderDrawParameters );

		// Implement RayQuery features.		
		featuresRayQuery.rayQuery = true;
		LOG_FEATURE( featuresRayQuery.rayQuery );

		// Implement AccelerationStructure features.
		featuresAccelerationStructure.accelerationStructure = true;
		LOG_FEATURE( featuresAccelerationStructure.accelerationStructure );

		// deviceCreateInfo.setPNext( &featuresChain );
		
		return physicalDevice.createDevice( deviceCreateInfo, nullptr );
	}

	vk::SurfaceKHR CreateSurfaceForWindow( const vk::Instance& instance, const HWND& hwnd ) {
		vk::Win32SurfaceCreateInfoKHR win32SurfaceCreateInfo = {};
		win32SurfaceCreateInfo.setHwnd(hwnd);
		win32SurfaceCreateInfo.setHinstance( GetModuleHandleA( nullptr ) );
	
		return instance.createWin32SurfaceKHR( win32SurfaceCreateInfo );
	}

	bool PhysicalDeviceHasExtensions( const vk::PhysicalDevice& device, const std::vector<const char*>& wantedExtensions ) {
		std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

		uint32_t foundExts{};
		for ( const char* wantedExt : wantedExtensions ) {
			for ( const vk::ExtensionProperties& ext : availableExtensions ) {
				if ( strcmp( ext.extensionName.data(), wantedExt ) == 0 ) {
					foundExts++;
				}
			}
		}

		return uint32_t( wantedExtensions.size() ) == foundExts;
	}

	vk::Format PhysicalDeviceFindDepthFormat( const vk::PhysicalDevice& physicalDevice ) {
		std::array<vk::Format, 3> wantedFormats = { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };

		vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
		vk::FormatFeatureFlags features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;

		for ( vk::Format format : wantedFormats ) {
			vk::FormatProperties props = physicalDevice.getFormatProperties( format );

			if ( tiling == vk::ImageTiling::eLinear && ( props.linearTilingFeatures & features ) == features ) {
				return format;
			} else if ( tiling == vk::ImageTiling::eOptimal && ( props.optimalTilingFeatures & features ) == features ) {
				return format;
			}
		}

		return vk::Format::eD32Sfloat;
	}

	uint32_t FindQueueFamilyIndex( const vk::SurfaceKHR& surface, const vk::PhysicalDevice& physicalDevice ) {
		std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();
		for ( size_t i = 0; i < queueFamilies.size(); i++ ) {
			vk::QueueFamilyProperties& family = queueFamilies[ i ];
			vk::Bool32 presentSupported = physicalDevice.getSurfaceSupportKHR( uint32_t( i ), surface );

			if ( family.queueFlags & vk::QueueFlagBits::eGraphics && family.queueFlags & vk::QueueFlagBits::eCompute && presentSupported )
				return uint32_t( i );
		}

		return vk::QueueFamilyIgnored;
	}

	SwapchainSupportData QuerySwapchainSupport( const vk::SurfaceKHR& surface, const vk::PhysicalDevice& physicalDevice ) {
		SwapchainSupportData out{};

		out.m_SurfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR( surface );
		out.m_SurfaceFormats = physicalDevice.getSurfaceFormatsKHR( surface );
		out.m_PresentModes = physicalDevice.getSurfacePresentModesKHR( surface );

		return out;
	}

	vk::SwapchainKHR CreateSwapchain( const vk::Device& device, const vk::PhysicalDevice& physicalDevice, const vk::SurfaceKHR& surface, const HWND& windowHandle, vk::Extent2D& outExtents, vk::Format& outImageFormat ) {
		const auto& swapChainSupport = QuerySwapchainSupport( surface, physicalDevice );
		if ( swapChainSupport.m_PresentModes.empty() || swapChainSupport.m_SurfaceFormats.empty() ) {
			printf( "Device does not have any present modes or surface formats\n" );
			return VK_NULL_HANDLE;
		}

		vk::SurfaceFormatKHR surfaceFormat = SwapchainSelectSurfaceFormat( swapChainSupport.m_SurfaceFormats );
		vk::PresentModeKHR presentMode = SwapchainSelectPresentMode( swapChainSupport.m_PresentModes );
		vk::Extent2D extent = SwapchainGetExtents( windowHandle, swapChainSupport.m_SurfaceCapabilities );

		uint32_t imageCount = swapChainSupport.m_SurfaceCapabilities.minImageCount + 1;
		if ( swapChainSupport.m_SurfaceCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.m_SurfaceCapabilities.maxImageCount ) {
			imageCount = swapChainSupport.m_SurfaceCapabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.setSurface( surface )
			.setMinImageCount( imageCount )
			.setImageFormat( surfaceFormat.format )
			.setImageColorSpace( surfaceFormat.colorSpace )
			.setImageExtent( extent )
			.setImageArrayLayers( 1 )
			.setImageUsage( vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled )
			.setImageSharingMode( vk::SharingMode::eExclusive );

		swapchainCreateInfo.setPreTransform( swapChainSupport.m_SurfaceCapabilities.currentTransform )
			.setCompositeAlpha( vk::CompositeAlphaFlagBitsKHR::eOpaque )
			.setPresentMode( presentMode )
			.setClipped( VK_TRUE );
		
		outExtents = extent;
		outImageFormat = surfaceFormat.format;

		return device.createSwapchainKHR( swapchainCreateInfo );
	}

	vk::SurfaceFormatKHR SwapchainSelectSurfaceFormat( const std::vector<vk::SurfaceFormatKHR>& availableFormats ) {
		for ( const auto& surfaceFormat : availableFormats ) {
			if ( surfaceFormat.format == vk::Format::eR8G8B8A8Unorm && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) {
				return surfaceFormat;
			}
		}

		return availableFormats[ 0 ];
	}

	vk::PresentModeKHR SwapchainSelectPresentMode( const std::vector<vk::PresentModeKHR>& availableModes ) {
		for ( const auto& mode : availableModes ) {
			if ( mode == vk::PresentModeKHR::eMailbox ) {
				return mode;
			}
		}

		return vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D SwapchainGetExtents( const HWND& windowHandle, const vk::SurfaceCapabilitiesKHR& capabilities ) {
		if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
			return capabilities.currentExtent;
		} else {
			RECT area{};
			GetClientRect( windowHandle, &area );

			vk::Extent2D actualExtent = {
				uint32_t( area.right ),
				uint32_t( area.bottom )
			};

			actualExtent.width = std::clamp( actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
			actualExtent.height = std::clamp( actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );

			return actualExtent;
		}
	}

	vk::ImageView CreateImageView( const vk::Device& device, const vk::Image& image, vk::ImageViewCreateInfo& createInfo ) {
		// TODO: Maybe stop doing this?
		createInfo.image = image;
		return device.createImageView( createInfo );
	}

	void CreateSwapchainImages( const vk::Device& device, const vk::SwapchainKHR& swapchain, const vk::Format imageFormat, std::vector<vk::Image>& outImages, std::vector<vk::ImageView>& outImageViews ) {
		outImages = device.getSwapchainImagesKHR(swapchain);
		outImageViews.resize( outImages.size() );

		for ( size_t i = 0; i < outImageViews.size(); i++ ) {
			vk::ImageViewCreateInfo imageViewCreateInfo = {};
			vk::ImageSubresourceRange subresourceRange( vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0,vk::RemainingArrayLayers );

			imageViewCreateInfo.setViewType( vk::ImageViewType::e2D )
				.setFormat( imageFormat )
				.setSubresourceRange( subresourceRange );

			outImageViews[ i ] = CreateImageView( device, outImages[ i ], imageViewCreateInfo );
		}
	}

	vk::RenderingAttachmentInfo RenderPassGetColorAttachmentInfo( const vk::ImageView& view, vk::ClearValue* clear, vk::ImageLayout layout, vk::ResolveModeFlagBits resolveMode, const vk::ImageView& resolveImage, vk::ImageLayout resolveLayout ) {
		vk::RenderingAttachmentInfo colorAttachment = {};

		colorAttachment.setImageView(view)
			.setImageLayout(layout)
			.setLoadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setResolveMode(resolveMode)
			.setResolveImageView(resolveImage)
			.setResolveImageLayout(resolveLayout);

		if(clear)
			colorAttachment.setClearValue(*clear);

		return colorAttachment;
	}

	vk::RenderingAttachmentInfo RenderPassGetDepthAttachmentInfo( const vk::ImageView& view, vk::ImageLayout layout ) {
		vk::RenderingAttachmentInfo depthAttachment = {};
		depthAttachment.setImageView(view)
			.setImageLayout(layout)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setClearValue( { vk::ClearDepthStencilValue{ 1.f, 0u } } );

		return depthAttachment;
	}

	vk::RenderingInfo RenderPassCreateRenderingInfo( vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachments, vk::RenderingAttachmentInfo* depthAttachment, uint32_t colorAttachmentCount ) {
		vk::RenderingInfo renderInfo = {};

		renderInfo.setRenderArea( vk::Rect2D{ vk::Offset2D{ 0, 0 }, renderExtent } )
			.setLayerCount(1);

		if ( colorAttachments ) {
			renderInfo.setColorAttachmentCount( colorAttachmentCount );
			renderInfo.setPColorAttachments( colorAttachments );
		}

		renderInfo.setPDepthAttachment( depthAttachment );

		return renderInfo;
	}

	vk::PipelineMultisampleStateCreateInfo PipelineDefaultMultiSampleState() {
		vk::PipelineMultisampleStateCreateInfo out = {};
		out.sampleShadingEnable = vk::False;
		out.rasterizationSamples = vk::SampleCountFlagBits::e1;
		return out;
	}

	vk::PipelineInputAssemblyStateCreateInfo PipelineDefaultInputAssemblyState() {
		vk::PipelineInputAssemblyStateCreateInfo out = {};
		out.topology = vk::PrimitiveTopology::eTriangleList;
		out.primitiveRestartEnable = vk::False;
		return out;
	}

	vk::PipelineRasterizationStateCreateInfo PipelineDefaultRasterizationState() {
		vk::PipelineRasterizationStateCreateInfo out = {};
		out.depthClampEnable = vk::False;
		out.rasterizerDiscardEnable = vk::False;
		out.polygonMode = vk::PolygonMode::eFill;
		out.lineWidth = 1.0f;
		out.cullMode = vk::CullModeFlagBits::eBack;
		out.frontFace = vk::FrontFace::eCounterClockwise;
		out.depthBiasEnable = vk::True;
		return out;
	}

	vk::PipelineDepthStencilStateCreateInfo PipelineDefaultDepthStencilState() {
		vk::PipelineDepthStencilStateCreateInfo out = {};
		out.depthTestEnable = vk::True;
		out.depthWriteEnable = vk::True;
		out.depthCompareOp = vk::CompareOp::eLess;
		return out;
	}

	vk::PipelineDynamicStateCreateInfo PipelineDefaultDynamicState() {
		static std::vector<vk::DynamicState> dynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
			vk::DynamicState::eCullMode,
			vk::DynamicState::eFrontFace,
		};

		vk::PipelineDynamicStateCreateInfo out = {};
		out.setDynamicStates( dynamicStates );
		return out;
	}

	vk::PipelineColorBlendAttachmentState PipelineDefaultColorBlendAttachmentState() {
		vk::PipelineColorBlendAttachmentState out = { };
		out.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		return out;
	}
}