#include "Pch.hpp"
#include "Mesh.hpp"

namespace Boundless {
	void Mesh::PackVertexData() {
		if(!m_Vertices.empty())
			return;

		bool hasUVs = !m_Texcoords.empty();
		bool hasNormals = !m_Normals.empty();
		bool hasTangents = !m_Tangents.empty();

		for ( auto i = 0; i < m_Positions.size(); i++ ) {
			MeshVertexData mvd = {};	
			mvd.m_Position = m_Positions[i];
		
			if ( hasUVs ) {
				mvd.m_UVx = m_Texcoords[i].x;
				mvd.m_UVy = m_Texcoords[i].y;
			}

			if ( hasNormals ) {
				mvd.m_Normal = m_Normals[i];
			}

			if ( hasTangents ) {
				mvd.m_Tangent = m_Tangents[i];
			}

			m_Vertices.push_back( mvd );
		}
	}

	void KeyFrame::LoadChannel( const AnimationChannel& channel ) { 
		if ( channel.m_Type == EChannelType::Translation )
			m_Translation = channel;
		if ( channel.m_Type == EChannelType::Rotation )
			m_Rotation = channel;
		if ( channel.m_Type == EChannelType::Scale )
			m_Scale = channel;
	}

	Animation::Animation( const tinygltf::Model& gltfModel, const tinygltf::Animation& gltfAnimation ) {
		m_Name = gltfAnimation.name;
	
		// Load channels.
		for ( size_t i = 0; i < gltfAnimation.channels.size(); i++ ) {
			const tinygltf::AnimationChannel& gltfChannel = gltfAnimation.channels[ i ];

			AnimationChannel channel = {};
			channel.m_Type = GetChannelType( gltfChannel.target_path );
			channel.m_Sampler = LoadSampler( gltfModel, gltfAnimation, gltfChannel );

			if ( !channel.m_Sampler.m_Inputs.empty() ) {
				auto [min_it, max_it] = std::ranges::minmax_element( channel.m_Sampler.m_Inputs );
				m_Start = std::min( m_Start, *min_it );
				m_End = std::max( m_End, *max_it );
			}

			std::string boneName = gltfModel.nodes[ gltfChannel.target_node ].name;
			m_KeyFrames[ boneName ].LoadChannel( channel );
		}
	}
	
	void Animation::Update( float deltaTime ) {
		if ( m_IsPaused )
			return;

		m_CurTime += deltaTime;

		if ( m_CurTime > m_End )
			m_CurTime = m_Start;
	}

	AnimationSampler Animation::LoadSampler( const tinygltf::Model& gltfModel, const tinygltf::Animation& gltfAnimation, const tinygltf::AnimationChannel& gltfAnimationChannel ) {
		const auto& gltfSampler = gltfAnimation.samplers[ gltfAnimationChannel.sampler ];

		AnimationSampler sampler = {};
		sampler.m_Interpolation = gltfSampler.interpolation;

		{
			const auto& accessor = gltfModel.accessors[ gltfSampler.input ];
			const auto& bufferView = gltfModel.bufferViews[ accessor.bufferView ];
			const auto& buffer = gltfModel.buffers[ bufferView.buffer ];
			const float* buf = reinterpret_cast< const float* >( &buffer.data[ accessor.byteOffset + bufferView.byteOffset ] );

			for ( size_t index = 0; index < accessor.count; index++ )
				sampler.m_Inputs.push_back( buf[ index ] );
		}

		{
			const auto& accessor = gltfModel.accessors[ gltfSampler.output ];
			const auto& bufferView = gltfModel.bufferViews[ accessor.bufferView ];
			const auto& buffer = gltfModel.buffers[ bufferView.buffer ];
			const void* data = &buffer.data[ accessor.byteOffset + bufferView.byteOffset ];

			if ( accessor.type == TINYGLTF_TYPE_VEC3 ) {
				const glm::vec3* buf = static_cast< const glm::vec3* >( data );
				for ( size_t index = 0; index < accessor.count; index++ )
					sampler.m_Outputs.push_back( glm::vec4( buf[ index ].x, buf[ index ].y, buf[ index ].z, 0.0f ) );
			}

			if ( accessor.type == TINYGLTF_TYPE_VEC4 ) {
				const glm::vec4* buf = static_cast< const glm::vec4* >( data );
				for ( size_t index = 0; index < accessor.count; index++ )
					sampler.m_Outputs.push_back( buf[ index ] );
			}
		}

		return sampler;
	}
	
	void Skeleton::UpdateBoneTransform( const Animation& animation, Bone& bone, const glm::mat4& parentTransform ) { 
		glm::mat4 globalTransform = glm::mat4( 1.f );

		bool hasAnimation = animation.m_KeyFrames.contains( bone.m_Name );
		if ( hasAnimation ) {
			const KeyFrame& keyframe = animation.m_KeyFrames.at( bone.m_Name );

			glm::vec3 translation = keyframe.m_Translation.InterpolateVec( animation.m_CurTime );
			glm::quat rotation = keyframe.m_Rotation.InterpolateQuat( animation.m_CurTime );
			glm::vec3 scale = keyframe.m_Scale.InterpolateVec( animation.m_CurTime );
	
			glm::mat4 T = glm::translate( glm::mat4( 1.f ), translation );
			glm::mat4 R = glm::mat4_cast( rotation );
			glm::mat4 S = glm::scale( glm::mat4( 1.f ), scale );
			
			glm::mat4 localTransform = T * R * S;

			globalTransform = parentTransform * localTransform;

			m_BoneWSTransformMatrices[ bone.m_Index ] = globalTransform;
			m_BoneTransformMatrices[ bone.m_Index ] = globalTransform * m_InverseBindMatrices[ bone.m_Index ];
		}
		
		for ( Bone& child : bone.m_Children )
			UpdateBoneTransform( animation, child, globalTransform );
	}
	
	void Skeleton::UpdateFromAnimation( const Animation& animation ) { 
		UpdateBoneTransform( animation, m_RootBone, glm::mat4( 1.f ) );
	}
}