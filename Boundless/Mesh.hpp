#pragma once
#include "VkUtil.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

namespace Boundless {
	struct alignas( 16 ) MeshVertexData {
		glm::vec3 m_Position{};
		float	  m_UVx{};
		glm::vec3 m_Normal{};
		float	  m_UVy{};
		glm::vec4 m_Tangent{};
	};

	struct Mesh {
		void PackVertexData();
	
		std::string					 m_Name;
		std::vector<glm::vec3>		 m_Positions;
		std::vector<glm::vec3>		 m_Normals;
		std::vector<glm::vec2>		 m_Texcoords;
		std::vector<glm::vec4>		 m_Tangents;
		std::vector<uint32_t>		 m_Indices;
		std::vector<MeshVertexData>  m_Vertices;

		uint32_t					 m_Material = 0; // TODO: Fix.
		BufferHandle				 m_IndexBuffer = BufferHandle::Invalid;
		BufferHandle				 m_VertexBuffer = BufferHandle::Invalid;
		
		// RT Data.
		BufferHandle				 m_BlasBuffer = BufferHandle::Invalid;
		vk::AccelerationStructureKHR m_Blas = {};
		
		// Skinning Data.
		std::vector<glm::vec4>		 m_BoneWeights;
		std::vector<glm::uvec4>		 m_BoneIndices;
		BufferHandle				 m_BoneIndexBuffer = BufferHandle::Invalid;
		BufferHandle				 m_BoneWeightBuffer = BufferHandle::Invalid;
		BufferHandle				 m_SkinnedVertexBuffer = BufferHandle::Invalid;
		entt::entity				 m_Skeleton;
	};

	enum class EAlphaMode : int32_t {
		None = -1,
		Opaque,
		Blend,
		Mask
	};

	struct alignas(16) GPUMaterial {
		int			m_Index{};
		glm::vec3	m_PadVec3{}; // TODO: Maybe use this space?
		float		m_MetallicFactor = 1.f;
		float		m_RoughnessFactor = 1.f;
		EAlphaMode	m_AlphaMode = EAlphaMode::Opaque;
		float		m_AlphaCutoff = 0.5f;
		ImageHandle m_AlbedoTexture = ImageHandle::Invalid;
		ImageHandle m_NormalsTexture = ImageHandle::Invalid;
		ImageHandle m_MetalRoughnessTexture = ImageHandle::Invalid;
		ImageHandle m_EmissiveTexture = ImageHandle::Invalid;
		glm::vec4	m_Albedo = {1.f, 1.f, 1.f, 1.f};
		glm::vec4	m_Emissive = {0.f, 0.f, 0.f, 1.f};
	};

	struct Material : GPUMaterial {
		std::string m_AlbedoTexturePath;
		std::string m_NormalsTexturePath;
		std::string m_MetalRoughnessTexturePath;
		std::string m_EmissiveTexturePath;
	};

	struct AnimationSampler {
		std::string			   m_Interpolation{};
		std::vector<float>	   m_Inputs{};
		std::vector<glm::vec4> m_Outputs{};
	};

	enum class EChannelType {
		None,
		Translation,
		Rotation,
		Scale
	};

	__forceinline EChannelType GetChannelType( const std::string& channelType ) {
		if ( channelType == "translation" )
			return EChannelType::Translation;
		if ( channelType == "rotation" )
			return EChannelType::Rotation;
		if ( channelType == "scale" )
			return EChannelType::Scale;

		return EChannelType::None;
	}

	struct AnimationChannel {
		EChannelType	 m_Type = EChannelType::None;
		AnimationSampler m_Sampler = {};

		glm::vec3 InterpolateVec( float time ) const {
			const auto& sampler = m_Sampler;

			if ( sampler.m_Inputs.size() == 1 )
				return glm::vec3( sampler.m_Outputs[ 0 ] );

			int index = 0;
			for ( ; index < sampler.m_Inputs.size() - 1; index++ ) {
				if ( ( time >= sampler.m_Inputs[ index ] ) && ( time <= sampler.m_Inputs[ index + 1 ] ) ) {
					break;
				}
			}

			float a = ( time - sampler.m_Inputs[ index ] ) / ( sampler.m_Inputs[ index + 1 ] - sampler.m_Inputs[ index ] );

			glm::vec3 t1 = sampler.m_Outputs[ index ];
			glm::vec3 t2 = sampler.m_Outputs[ index + 1 ];

			return glm::mix( t1, t2, a );
		}

		glm::quat InterpolateQuat( float time ) const {
			const auto& sampler = m_Sampler;

			if( sampler.m_Inputs.size() == 1 )
				return glm::quat( sampler.m_Outputs[ 0 ].w, sampler.m_Outputs[ 0 ].x, sampler.m_Outputs[ 0 ].y, sampler.m_Outputs[ 0 ].z );

			int index = 0;
			for ( ; index < sampler.m_Inputs.size() - 1; index++ ) {
				if ( ( time >= sampler.m_Inputs[ index ] ) && ( time <= sampler.m_Inputs[ index + 1 ] ) ) {
					break;
				}
			}

			float a = ( time - sampler.m_Inputs[ index ] ) / ( sampler.m_Inputs[ index + 1 ] - sampler.m_Inputs[ index ] );
			
			glm::quat q1( sampler.m_Outputs[ index ].w, sampler.m_Outputs[ index ].x, sampler.m_Outputs[ index ].y, sampler.m_Outputs[ index ].z );
			glm::quat q2( sampler.m_Outputs[ index + 1 ].w, sampler.m_Outputs[ index + 1 ].x, sampler.m_Outputs[ index + 1 ].y, sampler.m_Outputs[ index + 1 ].z );
			
			return glm::normalize( glm::slerp( q1, q2, a ) );
		}
	};

	struct KeyFrame {
		AnimationChannel m_Translation;
		AnimationChannel m_Rotation;
		AnimationChannel m_Scale;

		void LoadChannel( const AnimationChannel& channel );
	};

	class Animation {
		friend struct Skeleton;
	public:
		Animation() = default;
		Animation( const tinygltf::Model& gltfModel, const tinygltf::Animation& gltfAnimation );

		void Update( float deltaTime );
	private:
		AnimationSampler LoadSampler( const tinygltf::Model& gltfModel, const tinygltf::Animation& gltfAnimation, const tinygltf::AnimationChannel& gltfAnimationChannel );

		std::string								  m_Name;
		std::unordered_map<std::string, KeyFrame> m_KeyFrames;
		float									  m_Start = FLT_MAX;
		float									  m_End = FLT_MIN;
		float									  m_CurTime = 0.f;
		bool									  m_IsPaused = false;
	};

	struct Bone {
		uint32_t		  m_Index;
		std::string		  m_Name;
		std::vector<Bone> m_Children;
	};

	struct Skeleton {
		Bone					m_RootBone;
		entt::entity			m_Animation;
		std::vector<glm::mat4>  m_InverseBindMatrices;
		std::vector<glm::mat4>  m_BoneWSTransformMatrices;
		std::vector<glm::mat4>  m_BoneTransformMatrices;
		BufferHandle			m_BoneTransformsBuffer = BufferHandle::Invalid;

		void UpdateBoneTransform( const Animation& animation, Bone& bone, const glm::mat4& parentTransform );
		void UpdateFromAnimation( const Animation& animation );
	};
}