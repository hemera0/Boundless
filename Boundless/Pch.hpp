#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <intrin.h>
#include <windows.h>

#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>

#include <entt/entt.hpp>

// #include <volk/volk.h>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>
#include <stb_image/stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <tinyobjloader/tiny_obj_loader.h>
#include <tinygltf/tiny_gltf.h>

// TODO: add more stl aliases/utilities 
template<typename T>
struct VectorWrapper {
	VectorWrapper( const std::vector<T>& vec ) : m_Data( vec ) { }
	VectorWrapper( const T& elem ) : m_Data{ elem } { }

	operator std::vector<T>& () { return m_Data; }
	operator const std::vector<T>& ( ) const { return m_Data; }

	std::vector<T> m_Data;
};