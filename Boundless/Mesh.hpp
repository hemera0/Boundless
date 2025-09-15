#pragma once
#include "VkUtil.hpp"
#include "Buffer.hpp"
#include <glm/glm.hpp>

namespace Boundless {
	struct alignas(16) MeshVertexData {
		glm::vec3 m_Position{};
		float m_UVx{};
		glm::vec3 m_Normal{};
		float m_UVy{};
	};

	struct Mesh {
		std::vector<MeshVertexData> m_Vertices{};

		Buffer* m_VertexBuffer = nullptr;
	};

	enum class EAlphaMode : int32_t {
		None = -1,
		Opaque,
		Blend,
		Mask
	};

	struct alignas(16) Material {
		float m_MetallicFactor{};
		float m_RoughnessFactor{};
		EAlphaMode m_AlphaMode{};
		float m_AlphaCutoff{};

		int32_t m_AlbedoTexture{};
		int32_t m_NormalsTexture{};
		int32_t m_MetalRoughnessTexture{};
		int32_t m_EmissiveTexture{};

		glm::vec4 m_Albedo{};
		glm::vec4 m_Emissive{};
	};
}