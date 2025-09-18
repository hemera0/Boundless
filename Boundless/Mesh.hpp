#pragma once
#include "VkUtil.hpp"
#include "Buffer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Boundless {
	struct alignas( 16 ) MeshVertexData {
		glm::vec3 m_Position{};
		float m_UVx{};
		glm::vec3 m_Normal{};
		float m_UVy{};
	};

	struct Mesh {
		std::string m_Name{};

		std::vector<glm::vec3> m_Positions{};
		std::vector<glm::vec3> m_Normals{};
		std::vector<glm::vec2> m_Texcoords{};

		std::vector<uint32_t> m_Indices{};
		std::vector<MeshVertexData> m_Vertices{};

		uint32_t m_Material{}; 

		Buffer* m_IndexBuffer = nullptr;
		Buffer* m_VertexBuffer = nullptr;

		void PackVertexData();
	};

	enum class EAlphaMode : int32_t {
		None = -1,
		Opaque,
		Blend,
		Mask
	};

	struct alignas(16) GPUMaterial {
		int m_Index{};

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

	struct alignas( 16 ) Material : GPUMaterial {
		std::string m_AlbedoTexturePath{};
		std::string m_NormalsTexturePath{};
		std::string m_MetalRoughnessTexturePath{};
		std::string m_EmissiveTexturePath{};
	};

	struct AnimationSampler {
		std::string m_Interpolation{};
		std::vector<float> m_Inputs{};
		std::vector<glm::vec4> m_Outputs{};
	};

	enum class EChannelType {
		None,
		Translation,
		Rotation,
		Scale
	};

	struct AnimationChannel {
		EChannelType m_Type{};
		// entt::entity m_Node{};
		uint32_t m_Sampler{}; // Sampler Index.
	};

	struct Animation {
		std::string m_Name{};
		std::vector<AnimationSampler> m_Samplers{};
		std::vector<AnimationChannel> m_Channels{};

		float m_Start{ FLT_MAX }, m_End{ FLT_MIN };
		float m_CurTime{};

		bool m_IsPaused{};
	};
}