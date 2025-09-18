#include "VkUtil.hpp"

namespace VkUtil {
	constexpr const bool UseValidationLayers = true;

	VkInstance CreateInstance( const char** requiredExtensions, const int requiredExtensionsCount ) {
		VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.pEngineName = "Boundless";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_4;

		VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = nullptr;

		if ( requiredExtensions && requiredExtensionsCount ) {
			instanceCreateInfo.enabledExtensionCount = requiredExtensionsCount;
			instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions;
		}

		if constexpr ( UseValidationLayers ) {
			std::array<const char*, 1> validationLayers = {
				"VK_LAYER_KHRONOS_validation",
			};

			uint32_t layerCount = 0;
			vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

			std::vector<VkLayerProperties> availableLayers( layerCount );
			vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

			size_t supportedLayersCount = 0;

			for ( const auto& wantedLayer : validationLayers ) {
				for ( const auto& availableLayer : availableLayers ) {
					if ( strcmp( wantedLayer, availableLayer.layerName ) == 0 )
						supportedLayersCount++;
				}
			}

			instanceCreateInfo.enabledLayerCount = static_cast< uint32_t >( validationLayers.size() );
			instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}

		VkInstance ret = VK_NULL_HANDLE;
		vkCreateInstance( &instanceCreateInfo, nullptr, &ret );

		return ret;
	}

	VkPhysicalDevice GetPhysicalDevice( const VkInstance& instance, const std::vector<const char*>& wantedExtensions ) {
		uint32_t deviceCount{};

		vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr );
		if ( deviceCount == 0 ) {
			printf( "Failed to find vulkan enabled GPUs\n" );
			return VK_NULL_HANDLE;
		}

		std::vector<VkPhysicalDevice> devices( deviceCount );
		vkEnumeratePhysicalDevices( instance, &deviceCount, devices.data() );

		if ( deviceCount == 0 ) {
			printf( "Found no devices\n" );
			return VK_NULL_HANDLE;
		}

		printf( "Found %d physical devices\n", deviceCount );

		for ( const VkPhysicalDevice& device : devices ) {
			if ( !PhysicalDeviceIsDiscreteGPU( device ) || !PhysicalDeviceHasExtensions( device, wantedExtensions ) )
				continue;

			VkPhysicalDeviceProperties deviceProps{};
			vkGetPhysicalDeviceProperties( device, &deviceProps );
			printf( "Using %s\n", deviceProps.deviceName );

			return device;
		}

		return VK_NULL_HANDLE;
	}

	VkDevice CreateLogicalDevice( const VkInstance& instance, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const std::vector<const char*>& wantedExtensions ) {
		const auto& swapChainSupport = QuerySwapchainSupport( surface, physicalDevice );
		if ( swapChainSupport.m_PresentModes.empty() || swapChainSupport.m_SurfaceFormats.empty() ) {
			printf( "Device does not have any present modes or surface formats\n" );
			return VK_NULL_HANDLE;
		}

		const auto& queueFamilyIndices = FindQueueFamilyIndices( surface, physicalDevice );
		if ( !queueFamilyIndices.IsValid() ) {
			printf( "Failed to find valid queue families\n" );
			return VK_NULL_HANDLE;
		}

		float queuePriority = 1.0f;


		// TODO: Maybe make this its own function or something...
		// Query for feature support...
		VkPhysicalDeviceVulkan14Features testFeatures14{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
		VkPhysicalDeviceVulkan13Features testFeatures13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		VkPhysicalDeviceVulkan12Features testFeatures12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		VkPhysicalDeviceFeatures2 testFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

		testFeatures.pNext = &testFeatures14;
		testFeatures14.pNext = &testFeatures13;
		testFeatures13.pNext = &testFeatures12;

		vkGetPhysicalDeviceFeatures2( physicalDevice, &testFeatures );

		VkPhysicalDeviceFeatures deviceFeatures{};

		if ( testFeatures.features.samplerAnisotropy )
			deviceFeatures.samplerAnisotropy = VK_TRUE;
		if ( testFeatures.features.multiDrawIndirect )
			deviceFeatures.multiDrawIndirect = VK_TRUE;
		if ( testFeatures.features.depthClamp )
			deviceFeatures.depthClamp = VK_TRUE;
		if ( testFeatures.features.drawIndirectFirstInstance )
			deviceFeatures.drawIndirectFirstInstance = VK_TRUE;

		// TODO: Verify this is correct?
		if ( testFeatures12.shaderFloat16 )
			deviceFeatures.shaderInt16 = VK_TRUE;

		VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		deviceCreateInfo.enabledExtensionCount = static_cast< uint32_t >( wantedExtensions.size() );
		deviceCreateInfo.ppEnabledExtensionNames = wantedExtensions.data();

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { queueFamilyIndices.m_GraphicsFamilyIndex, queueFamilyIndices.m_PresentFamilyIndex };

		for ( const uint32_t familyIndex : uniqueQueueFamilies ) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = familyIndex;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		deviceCreateInfo.queueCreateInfoCount = static_cast< uint32_t >( queueCreateInfos.size() );
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

		// Shouldn't matter as this should match the VkInstance layers
		deviceCreateInfo.enabledLayerCount = 0;

	#define LOG_FEATURE(x) printf("\t" #x ": %s\n", x ? "true" : "false");

		// Implement 1.4 features.
		VkPhysicalDeviceVulkan14Features features14 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
		features14.maintenance5 = testFeatures14.maintenance5;

		LOG_FEATURE( features14.maintenance5 );

		// Implement 1.3 features.
		VkPhysicalDeviceVulkan13Features features13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		features13.dynamicRendering = testFeatures13.dynamicRendering;
		features13.synchronization2 = testFeatures13.synchronization2;

		LOG_FEATURE( features13.dynamicRendering );
		LOG_FEATURE( features13.synchronization2 );

		features14.pNext = &features13;

		// Implement 1.2 features.
		VkPhysicalDeviceVulkan12Features features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		features12.shaderFloat16 = testFeatures12.shaderFloat16;
		features12.descriptorIndexing = testFeatures12.descriptorIndexing;
		features12.shaderSampledImageArrayNonUniformIndexing = testFeatures12.shaderSampledImageArrayNonUniformIndexing;
		features12.shaderStorageBufferArrayNonUniformIndexing = testFeatures12.shaderStorageBufferArrayNonUniformIndexing;
		features12.shaderUniformBufferArrayNonUniformIndexing = testFeatures12.shaderUniformBufferArrayNonUniformIndexing;
		features12.runtimeDescriptorArray = testFeatures12.runtimeDescriptorArray;
		features12.descriptorBindingVariableDescriptorCount = testFeatures12.descriptorBindingVariableDescriptorCount;
		features12.descriptorBindingPartiallyBound = testFeatures12.descriptorBindingPartiallyBound;
		features12.bufferDeviceAddress = testFeatures12.bufferDeviceAddress;

		LOG_FEATURE( features12.shaderFloat16 );
		LOG_FEATURE( features12.descriptorIndexing );
		LOG_FEATURE( features12.shaderSampledImageArrayNonUniformIndexing );
		LOG_FEATURE( features12.shaderStorageBufferArrayNonUniformIndexing );
		LOG_FEATURE( features12.shaderUniformBufferArrayNonUniformIndexing );
		LOG_FEATURE( features12.runtimeDescriptorArray );
		LOG_FEATURE( features12.descriptorBindingVariableDescriptorCount );
		LOG_FEATURE( features12.descriptorBindingPartiallyBound );
		LOG_FEATURE( features12.bufferDeviceAddress );

		features13.pNext = &features12;

		VkPhysicalDeviceVulkan11Features features11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		features11.shaderDrawParameters = true;

		features12.pNext = &features11;

		// Implement Descriptor Buffer.
		// VkPhysicalDeviceDescriptorBufferFeaturesEXT featuresDescriptorBuffer = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
		// featuresDescriptorBuffer.descriptorBuffer = VK_TRUE;
		// featuresDescriptorBuffer.pNext = nullptr;

		deviceCreateInfo.pNext = &features14;

		VkDevice ret = VK_NULL_HANDLE;
		VkResult res = vkCreateDevice( physicalDevice, &deviceCreateInfo, nullptr, &ret );

		if ( res != VK_SUCCESS ) {
			printf( "Failed to create VkDevice: %d\n", res );
		}

		printf( "Device: 0x%p\n", ret );

		return ret;
	}

	VkSurfaceKHR CreateSurfaceForWindow( const VkInstance& instance, const HWND& hwnd ) {
		VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
		win32SurfaceCreateInfo.hwnd = hwnd;
		win32SurfaceCreateInfo.hinstance = GetModuleHandleA( nullptr ); // This might be bad?

		VkSurfaceKHR ret = VK_NULL_HANDLE;
		VkResult result = vkCreateWin32SurfaceKHR( instance, &win32SurfaceCreateInfo, nullptr, &ret );
		if ( result != VK_SUCCESS ) {
			printf( "Failed to create VkSurface: 0x%x\n", result );
		}

		return ret;
	}

	bool PhysicalDeviceIsDiscreteGPU( const VkPhysicalDevice& physicalDevice ) {
		VkPhysicalDeviceProperties deviceProps{};
		vkGetPhysicalDeviceProperties( physicalDevice, &deviceProps );
		return deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	}

	bool PhysicalDeviceHasExtensions( const VkPhysicalDevice& device, const std::vector<const char*>& wantedExtensions ) {
		uint32_t extensionCount{};
		vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );

		std::vector<VkExtensionProperties> availableExtensions( extensionCount );
		vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

		uint32_t foundExts{};

		for ( const char* wantedExt : wantedExtensions ) {
			for ( const VkExtensionProperties& ext : availableExtensions ) {
				if ( strcmp( ext.extensionName, wantedExt ) == 0 ) {
					foundExts++;
				}
			}
		}

		return static_cast< uint32_t >( wantedExtensions.size() ) == foundExts;
	}

	uint32_t PhysicalDeviceFindMemoryType( const VkPhysicalDevice& physicalDevice, const uint32_t filter, const VkMemoryPropertyFlags propertyFlags ) {
		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties( physicalDevice, &memProperties );

		for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ ) {
			if ( filter & ( 1 << i ) && ( memProperties.memoryTypes[ i ].propertyFlags & propertyFlags ) == propertyFlags ) {
				return i;
			}
		}

		return 0u;
	}

	VkFormat PhysicalDeviceFindDepthFormat( const VkPhysicalDevice& physicalDevice ) {
		std::array<VkFormat, 3> wantedFormats = {
			VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
		};

		VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

		for ( VkFormat format : wantedFormats ) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties( physicalDevice, format, &props );

			if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) {
				return format;
			} else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) {
				return format;
			}
		}

		return VK_FORMAT_D32_SFLOAT;
	}

	QueueFamilyIndices_t FindQueueFamilyIndices( const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice ) {
		QueueFamilyIndices_t out{};

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &queueFamilyCount, nullptr );

		std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &queueFamilyCount, queueFamilies.data() );

		for ( auto i{ 0u }; i < queueFamilyCount; i++ ) {
			if ( queueFamilies[ i ].queueFlags & VK_QUEUE_GRAPHICS_BIT )
				out.m_GraphicsFamilyIndex = i;
			if ( queueFamilies[ i ].queueFlags & VK_QUEUE_COMPUTE_BIT )
				out.m_ComputeFamilyIndex = i;

			VkBool32 presentSupported = false;
			vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, i, surface, &presentSupported );

			if ( presentSupported )
				out.m_PresentFamilyIndex = i;

			if ( out.IsValid() )
				break;
		}

		return out;
	}

	SwaphchainSupportData_t QuerySwapchainSupport( const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice ) {
		SwaphchainSupportData_t out{};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, surface, &out.m_SurfaceCapabilities );

		uint32_t formatCount{};
		vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, nullptr );

		if ( formatCount != 0 ) {
			out.m_SurfaceFormats.resize( formatCount );
			vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, out.m_SurfaceFormats.data() );
		}

		uint32_t presentModeCount{};
		vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, nullptr );

		if ( presentModeCount != 0 ) {
			out.m_PresentModes.resize( presentModeCount );
			vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, out.m_PresentModes.data() );
		}

		return out;
	}

	VkQueue DeviceGetQueueByIndex( const VkDevice& device, uint32_t queueIndex ) {
		VkQueue ret = VK_NULL_HANDLE;
		vkGetDeviceQueue( device, queueIndex, 0, &ret );
		return ret;
	}

	VkCommandPool CreateCommandPool( const VkDevice& device, const VkCommandPoolCreateInfo& createInfo ) {
		VkCommandPool ret = VK_NULL_HANDLE;
		VkResult res = vkCreateCommandPool( device, &createInfo, nullptr, &ret );

		if ( res != VK_SUCCESS ) {
			printf( "Failed to create CommandPool: %d\n", res );
		}

		return ret;
	}

	VkCommandBuffer CreateCommandBuffer( const VkDevice& device, const VkCommandPool& commandPool ) {
		VkCommandBufferAllocateInfo commandBufferAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		commandBufferAllocInfo.commandPool = commandPool;
		commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocInfo.commandBufferCount = 1;

		VkCommandBuffer ret = VK_NULL_HANDLE;
		VkResult result = vkAllocateCommandBuffers( device, &commandBufferAllocInfo, &ret );
		if ( result != VK_SUCCESS ) {
			printf( "Failed to allocate CommandBuffer: %d\n", result );
		}

		return ret;
	}

	VkSwapchainKHR CreateSwapchain( const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const HWND& windowHandle, VkExtent2D& outExtents ) {
		const auto& swapChainSupport = QuerySwapchainSupport( surface, physicalDevice );
		if ( swapChainSupport.m_PresentModes.empty() || swapChainSupport.m_SurfaceFormats.empty() ) {
			printf( "Device does not have any present modes or surface formats\n" );
			return VK_NULL_HANDLE;
		}

		const auto familyIndices = FindQueueFamilyIndices( surface, physicalDevice );
		if ( !familyIndices.IsValid() ) {
			printf( "Failed to find valid queue families\n" );
			return VK_NULL_HANDLE;
		}

		// TODO: Verify we can sample from the swapchain images.
		VkSurfaceFormatKHR surfaceFormat = SwapchainSelectSurfaceFormat( swapChainSupport.m_SurfaceFormats );
		VkPresentModeKHR presentMode = SwapchainSelectPresentMode( swapChainSupport.m_PresentModes );
		VkExtent2D extent = SwapchainGetExtents( windowHandle, swapChainSupport.m_SurfaceCapabilities );

		uint32_t imageCount = swapChainSupport.m_SurfaceCapabilities.minImageCount + 1;
		if ( swapChainSupport.m_SurfaceCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.m_SurfaceCapabilities.maxImageCount ) {
			imageCount = swapChainSupport.m_SurfaceCapabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR swapchainCreateInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		swapchainCreateInfo.surface = surface;
		swapchainCreateInfo.minImageCount = imageCount;
		swapchainCreateInfo.imageFormat = surfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapchainCreateInfo.imageExtent = extent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		uint32_t queueFamilyIndices[ ] = { familyIndices.m_GraphicsFamilyIndex, familyIndices.m_PresentFamilyIndex };
		if ( familyIndices.m_GraphicsFamilyIndex != familyIndices.m_PresentFamilyIndex ) {
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.queueFamilyIndexCount = 0; // Optional
			swapchainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		swapchainCreateInfo.preTransform = swapChainSupport.m_SurfaceCapabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = presentMode;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		VkSwapchainKHR ret = VK_NULL_HANDLE;
		const auto result = vkCreateSwapchainKHR( device, &swapchainCreateInfo, nullptr, &ret );
		if ( result != VK_SUCCESS ) {
			printf( "Failed to create swapchain %d\n", result );
			return VK_NULL_HANDLE;
		}

		outExtents = extent;

		return ret;
	}

	VkSurfaceFormatKHR SwapchainSelectSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats ) {
		for ( const auto& surfaceFormat : availableFormats ) {
			if ( surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
				return surfaceFormat;
			}
		}

		return availableFormats[ 0 ];
	}

	VkPresentModeKHR SwapchainSelectPresentMode( const std::vector<VkPresentModeKHR>& availableModes ) {
		for ( const auto& mode : availableModes ) {
			if ( mode == VK_PRESENT_MODE_MAILBOX_KHR ) {
				return mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D SwapchainGetExtents( const HWND& windowHandle, const VkSurfaceCapabilitiesKHR& capabilities ) {
		if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
			return capabilities.currentExtent;
		} else {
			RECT area{};
			GetClientRect( windowHandle, &area );

			VkExtent2D actualExtent = {
				static_cast< uint32_t >( area.right ),
				static_cast< uint32_t >( area.bottom )
			};

			actualExtent.width = std::clamp( actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
			actualExtent.height = std::clamp( actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );

			return actualExtent;
		}
	}

	VkImageView CreateImageView( const VkDevice& device, const VkImage& image, VkImageViewCreateInfo* createInfo ) {
		VkImageView ret = VK_NULL_HANDLE;
		if ( !createInfo )
			return ret;

		createInfo->image = image;

		VkResult result = vkCreateImageView( device, createInfo, nullptr, &ret );
		if ( result != VK_SUCCESS ) {
			printf( "Failed to create image view: %d\n", result );
			return VK_NULL_HANDLE;
		}

		return ret;
	}

	VkFormat SwapchainGetImageFormat( const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface ) {
		const auto& swapChainSupport = QuerySwapchainSupport( surface, physicalDevice );
		if ( swapChainSupport.m_PresentModes.empty() || swapChainSupport.m_SurfaceFormats.empty() ) {
			printf( "Device does not have any present modes or surface formats\n" );
			return VK_FORMAT_UNDEFINED;
		}

		return SwapchainSelectSurfaceFormat( swapChainSupport.m_SurfaceFormats ).format;
	}

	void CreateSwapchainImages( const VkDevice& device, const VkSwapchainKHR& swapchain, const VkFormat imageFormat, std::vector<VkImage>& outImages, std::vector<VkImageView>& outImageViews ) {
		uint32_t imageCount = 0;
		vkGetSwapchainImagesKHR( device, swapchain, &imageCount, nullptr );

		outImages.resize( imageCount );
		vkGetSwapchainImagesKHR( device, swapchain, &imageCount, outImages.data() );

		outImageViews.resize( imageCount );
		for ( auto i{ 0u }; i < outImageViews.size(); i++ ) {

			VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = imageFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;

			outImageViews[ i ] = CreateImageView( device, outImages[ i ], &imageViewCreateInfo );
		}
	}

	void CommandBufferBegin( const VkCommandBuffer& commandBuffer, VkCommandBufferUsageFlags flags ) {
		VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

		if ( flags != 0 )
			commandBufferBeginInfo.flags = flags;

		commandBufferBeginInfo.pInheritanceInfo = nullptr; // Optional

		VkResult result = vkBeginCommandBuffer( commandBuffer, &commandBufferBeginInfo );
		if ( result != VK_SUCCESS ) {
			printf( "failed to begin command buffer %d\n", result );
		}
	}

	void CommandBufferEnd( const VkCommandBuffer& commandBuffer ) {
		vkEndCommandBuffer( commandBuffer );
	}

	void CommandBufferSubmit( const VkCommandBuffer& commandBuffer, const VkQueue& queue, bool wait ) {
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit( queue, 1, &submitInfo, VK_NULL_HANDLE );

		if ( wait )
			vkQueueWaitIdle( queue );
	}

	void CommandBufferCopyBufferToImage( const VkCommandBuffer& commandBuffer, const VkBuffer& buffer, const VkImage& image, uint32_t width, uint32_t height ) {
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage( commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	}

	void CommandBufferImageBarrier( const VkCommandBuffer& commandBuffer, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange ) {
		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.oldLayout = oldImageLayout;
		barrier.newLayout = newImageLayout;
		barrier.image = image;
		barrier.subresourceRange = subresourceRange;

		vkCmdPipelineBarrier(
			commandBuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	void CommandBufferBufferBarrier( const VkCommandBuffer& commandBuffer, VkBuffer buffer, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask ) {

	}

	void CommandBufferBeginRendering( const VkCommandBuffer& commandBuffer, VkRenderingInfo* renderingInfo ) {
		vkCmdBeginRendering( commandBuffer, renderingInfo );
	}

	void CommandBufferEndRendering( const VkCommandBuffer& commandBuffer ) {
		vkCmdEndRendering( commandBuffer );
	}

	void CommandBufferSetScissorAndViewport( const VkCommandBuffer& commandBuffer, float width, float height, float x, float y, float minDepth, float maxDepth ) {
		VkRect2D scissor{};
		scissor.offset = { int32_t( x ), int32_t( y ) };
		scissor.extent.width = uint32_t( width );
		scissor.extent.height = uint32_t( height );
		vkCmdSetScissor( commandBuffer, 0, 1, &scissor );

		VkViewport viewport{};
		viewport.x = x;
		viewport.y = height;
		viewport.width = width;
		viewport.height = -height; // flip y
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		vkCmdSetViewport( commandBuffer, 0, 1, &viewport );
	}

	VkRenderingAttachmentInfo RenderPassGetColorAttachmentInfo( const VkImageView view, VkClearValue* clear, VkImageLayout layout, VkResolveModeFlagBits resolveMode, VkImageView resolveImage, VkImageLayout resolveLayout ) {
		VkRenderingAttachmentInfo colorAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

		colorAttachment.imageView = view;
		colorAttachment.imageLayout = layout;
		colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		if ( clear ) {
			colorAttachment.clearValue = *clear;
		}

		colorAttachment.resolveMode = resolveMode;
		colorAttachment.resolveImageView = resolveImage;
		colorAttachment.resolveImageLayout = resolveLayout;

		return colorAttachment;
	}

	VkRenderingAttachmentInfo RenderPassGetDepthAttachmentInfo( const VkImageView view, VkImageLayout layout ) {
		VkRenderingAttachmentInfo depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

		depthAttachment.imageView = view;
		depthAttachment.imageLayout = layout;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.clearValue.depthStencil = { 0.f, 0u };

		return depthAttachment;
	}

	VkRenderingInfo RenderPassCreateRenderingInfo( VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachments, VkRenderingAttachmentInfo* depthAttachment, uint32_t colorAttachmentCount ) {
		VkRenderingInfo renderInfo{};
		renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderInfo.pNext = nullptr;

		renderInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, renderExtent };
		renderInfo.layerCount = 1;

		if ( colorAttachments ) {
			renderInfo.colorAttachmentCount = colorAttachmentCount;
			renderInfo.pColorAttachments = colorAttachments;
		} else {
			renderInfo.colorAttachmentCount = 0;
			renderInfo.pColorAttachments = nullptr;
		}

		if ( depthAttachment ) {
			renderInfo.pDepthAttachment = depthAttachment;
		}

		renderInfo.pStencilAttachment = nullptr;

		return renderInfo;
	}

	VkPipelineMultisampleStateCreateInfo PipelineDefaultMultiSampleState() {
		VkPipelineMultisampleStateCreateInfo out{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		out.sampleShadingEnable = VK_FALSE;
		out.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		out.minSampleShading = 1.0f; // Optional
		out.pSampleMask = nullptr; // Optional
		out.alphaToCoverageEnable = VK_FALSE; // Optional
		out.alphaToOneEnable = VK_FALSE; // Optional
		return out;
	}

	VkPipelineInputAssemblyStateCreateInfo PipelineDefaultInputAssemblyState() {
		VkPipelineInputAssemblyStateCreateInfo out{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		out.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		out.primitiveRestartEnable = VK_FALSE;
		return out;
	}

	VkPipelineRasterizationStateCreateInfo PipelineDefaultRasterizationState() {
		VkPipelineRasterizationStateCreateInfo out{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		out.depthClampEnable = VK_FALSE;
		out.rasterizerDiscardEnable = VK_FALSE;
		out.polygonMode = VK_POLYGON_MODE_FILL;
		out.lineWidth = 1.0f;
		out.cullMode = VK_CULL_MODE_NONE;
		out.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		out.depthBiasEnable = VK_FALSE;

		return out;
	}

	VkPipelineDepthStencilStateCreateInfo PipelineDefaultDepthStencilState() {
		VkPipelineDepthStencilStateCreateInfo out{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		out.depthTestEnable = VK_TRUE;
		out.depthWriteEnable = VK_TRUE;
		out.depthCompareOp = VK_COMPARE_OP_GREATER;
		// out.depthBoundsTestEnable = VK_FALSE;
		// out.minDepthBounds = 0.0f;
		// out.maxDepthBounds = 1.0f;
		// out.stencilTestEnable = VK_FALSE;
		return out;
	}

	VkPipelineDynamicStateCreateInfo PipelineDefaultDynamicState() {
		static std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo out{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		out.dynamicStateCount = static_cast< uint32_t >( dynamicStates.size() );
		out.pDynamicStates = dynamicStates.data();
		return out;
	}

	VkPipelineColorBlendAttachmentState PipelineDefaultColorBlendAttachmentState() {
		VkPipelineColorBlendAttachmentState out = { };
		out.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		out.blendEnable = VK_TRUE;
		out.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		out.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		out.colorBlendOp = VK_BLEND_OP_ADD;
		out.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		out.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		out.alphaBlendOp = VK_BLEND_OP_ADD;
		return out;
	}

	VkSampler CreateSampler( const VkDevice& device, VkSamplerAddressMode addressMode, VkFilter filter, float anisotropy, float minLod, float maxLod ) {
		VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.flags = 0;
		samplerInfo.addressModeU = addressMode;
		samplerInfo.addressModeV = addressMode;
		samplerInfo.addressModeW = addressMode;
		samplerInfo.minFilter = filter;
		samplerInfo.magFilter = filter;
		samplerInfo.mipmapMode = ( filter == VK_FILTER_NEAREST ) ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0;
		samplerInfo.minLod = minLod;
		samplerInfo.maxLod = maxLod;
		samplerInfo.anisotropyEnable = ( anisotropy >= 1.0f ) ? VK_TRUE : VK_FALSE;
		samplerInfo.maxAnisotropy = anisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_LESS;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.pNext = nullptr;

		VkSampler ret = VK_NULL_HANDLE;
		vkCreateSampler( device, &samplerInfo, nullptr, &ret );

		return ret;
	}
}