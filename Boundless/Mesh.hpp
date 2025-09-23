#pragma once
#include "VkUtil.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Boundless {
	struct alignas( 16 ) MeshVertexData {
		glm::vec3 m_Position{};
		float m_UVx{};
		glm::vec3 m_Normal{};
		float m_UVy{};
		glm::vec4 m_Tangent{};
	};

	struct Mesh {
		std::string m_Name{};

		std::vector<glm::vec3> m_Positions{};
		std::vector<glm::vec3> m_Normals{};
		std::vector<glm::vec2> m_Texcoords{};
		std::vector<glm::vec4> m_Tangents{};

		std::vector<uint32_t> m_Indices{};
		std::vector<MeshVertexData> m_Vertices{};

		uint32_t m_Material{}; 

		BufferHandle m_IndexBuffer{};
		BufferHandle m_VertexBuffer{};
		BufferHandle m_BlasBuffer{};
		VkAccelerationStructureKHR m_Blas{};

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
		glm::vec3 m_Pad{};

		float m_MetallicFactor{ 1.f };
		float m_RoughnessFactor{ 1.f };
		EAlphaMode m_AlphaMode{};
		float m_AlphaCutoff{};

		ImageHandle m_AlbedoTexture{};
		ImageHandle m_NormalsTexture{};
		ImageHandle m_MetalRoughnessTexture{};
		ImageHandle m_EmissiveTexture{};

		glm::vec4 m_Albedo{1.f, 1.f, 1.f, 1.f};
		glm::vec4 m_Emissive{0.f, 0.f, 0.f, 1.f};
	};

	struct alignas( 16 ) Material : GPUMaterial {
		std::string m_AlbedoTexturePath{};
		std::string m_NormalsTexturePath{};
		std::string m_MetalRoughnessTexturePath{};
		std::string m_EmissiveTexturePath{};

		SamplerDesc m_AlbedoSampler{};
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