#include "Pch.hpp"
#include "GLTFImporter.hpp"
#include "Transform.hpp"

namespace Boundless {
	GLTFImporter::GLTFImporter( Scene& inScene ) : m_Scene( inScene ) {}

	bool GLTFImporter::LoadFromFile( const std::string& path ) {
		tinygltf::TinyGLTF loader{};
		tinygltf::Model model{};
		std::string err{};
		std::string warn{};
	
		m_BaseDir = std::filesystem::path( path ).remove_filename().string();

		if ( !loader.LoadASCIIFromFile( &model, &err, &warn, path ) ) {
			if ( !warn.empty() )
				printf( "[GLTF] warn: %s\n", warn.c_str() );

			if(!err.empty())
				printf("[GLTF] error: %s\n", err.c_str());

			return false;
		}

		auto& registry = m_Scene.GetRegistry();

		std::string filename = std::filesystem::path( path ).filename().string();
		entt::entity gltfEntity = m_Scene.CreateEntityWithTransform( filename );

		// TODO: Maybe parent these to the gltfEntity?
		LoadMaterials( model );
		LoadAnimations( model );

		// Load skins.
		for(auto i = 0; i < model.skins.size(); i++) {
			LoadSkin( model, model.skins[ i ] );		
		}

		const auto& scene = model.scenes[ model.defaultScene == -1 ? 0 : model.defaultScene ];
		for ( auto i = 0; i < scene.nodes.size(); i++ ) {
			LoadNode( gltfEntity, model, model.nodes[ scene.nodes[ i ] ], glm::mat4( 1.f ) );
		}

		return true;
	}

	void GLTFImporter::LoadMesh( entt::entity entity, const tinygltf::Model& model, const tinygltf::Node& node, const tinygltf::Primitive& primitive ) {
		auto& registry = m_Scene.GetRegistry();
		auto& mesh = registry.emplace<Mesh>(entity);
	
		// Attach Skeleton to mesh.
		if ( node.skin > -1 )
			mesh.m_Skeleton = m_Skins[ node.skin ];

		mesh.m_Material = primitive.material;

		// Load vertex attributes.
		for ( const auto& [ type, index ] :  primitive.attributes ) {
			if ( index < 0 )
				continue;

			const auto& accessor   = model.accessors[ index ];
			const auto& bufferView = model.bufferViews[ accessor.bufferView ];
			const float* buffer	   = reinterpret_cast< const float* >( &( model.buffers[ bufferView.buffer ].data[ accessor.byteOffset + bufferView.byteOffset ] ) );

			for( auto i = 0; i < accessor.count; i++ ) {
				if ( type == "POSITION" ) {
					mesh.m_Positions.push_back( glm::vec3( buffer[ 0 ], buffer[ 1 ], buffer[ 2 ] ) );
				} if ( type == "NORMAL" ) {
					mesh.m_Normals.push_back( glm::vec3( buffer[ 0 ], buffer[ 1 ], buffer[ 2 ] ) );
				} if ( type == "TEXCOORD_0" ) {
					mesh.m_Texcoords.push_back( glm::vec2( buffer[ 0 ], buffer[ 1 ] ) );
				} if ( type == "TANGENT" ) {
					mesh.m_Tangents.push_back( glm::vec4( buffer[ 0 ], buffer[ 1 ], buffer[ 2 ], buffer[ 3 ] ) );
				}
			
				buffer += tinygltf::GetNumComponentsInType(accessor.type);
			}
		}

		// Load skinning attributes. (Seperate pass as they differ in buffer sizes)
		for ( const auto& [type, index] : primitive.attributes ) {
			if ( index < 0 )
				continue;

			const auto& accessor = model.accessors[ index ];
			const auto& bufferView = model.bufferViews[ accessor.bufferView ];
			const auto& buffer = model.buffers[ bufferView.buffer ];
			const uint8_t* ptr = reinterpret_cast< const uint8_t* >( &( buffer.data[ accessor.byteOffset + bufferView.byteOffset ] ) );

			for ( auto i = 0; i < accessor.count; i++ ) {
				if ( type == "JOINTS_0" ) {
					if ( accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ) {
						const uint8_t* ubuffer = reinterpret_cast< const uint8_t* >( ptr );
						mesh.m_BoneIndices.push_back( glm::uvec4( ubuffer[ 0 ], ubuffer[ 1 ], ubuffer[ 2 ], ubuffer[ 3 ] ) );
					}

					if ( accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ) {
						const uint16_t* ubuffer = reinterpret_cast< const uint16_t* >( ptr );
						mesh.m_BoneIndices.push_back( glm::uvec4( ubuffer[ 0 ], ubuffer[ 1 ], ubuffer[ 2 ], ubuffer[ 3 ] ) );
					}
				}

				if ( type == "WEIGHTS_0" ) {
					const float* fbuffer = reinterpret_cast< const float* >( ptr );
					mesh.m_BoneWeights.push_back( glm::vec4( fbuffer[ 0 ], fbuffer[ 1 ], fbuffer[ 2 ], fbuffer[ 3 ] ) );
				}

				ptr += tinygltf::GetNumComponentsInType( accessor.type ) * tinygltf::GetComponentSizeInBytes( accessor.componentType );
			}
		}

		if ( primitive.indices > -1 ) {
			const auto& accessor   = model.accessors[ primitive.indices ];
			const auto& bufferView = model.bufferViews[ accessor.bufferView ];
			const uint32_t* buffer = reinterpret_cast< const uint32_t* >( &( model.buffers[ bufferView.buffer ].data[ accessor.byteOffset + bufferView.byteOffset ] ) );

			for ( uint32_t i = 0; i < accessor.count; i++ ) {
				if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT ) {
					mesh.m_Indices.push_back( buffer[ i ] );
				}

				if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT ) {
					const uint16_t* buf = reinterpret_cast< const uint16_t* >( buffer );
					mesh.m_Indices.push_back( buf[ i ] );
				}

				if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE ) {
					const uint8_t* buf = reinterpret_cast< const uint8_t* >( buffer );
					mesh.m_Indices.push_back( buf[ i ] );
				}
			}
		}

		mesh.PackVertexData();
	}

	void GLTFImporter::LoadMaterials( const tinygltf::Model& model ) {
		auto& registry = m_Scene.GetRegistry();

		// TODO: Redo this parsing (fix multiple models)
		for ( auto i = 0 ; i < model.materials.size(); i++ ) {
			const auto& srcMaterial = model.materials[ i ];

			entt::entity matEntity = registry.create();

			Material& dstMaterial = registry.emplace<Material>(matEntity);
			dstMaterial.m_Index = i;

			if ( srcMaterial.alphaMode == "OPAQUE" )
				dstMaterial.m_AlphaMode = EAlphaMode::Opaque;
			if ( srcMaterial.alphaMode == "BLEND" )
				dstMaterial.m_AlphaMode = EAlphaMode::Blend;
			if ( srcMaterial.alphaMode == "MASK" )
				dstMaterial.m_AlphaMode = EAlphaMode::Mask;

			const auto& pbrMetallicRoughness = srcMaterial.pbrMetallicRoughness;
			const auto& emissiveFactor = srcMaterial.emissiveFactor;
			const auto& baseColorFactor = pbrMetallicRoughness.baseColorFactor;

			dstMaterial.m_Albedo		  = glm::vec4( baseColorFactor[ 0 ], baseColorFactor[ 1 ], baseColorFactor[ 2 ], baseColorFactor[ 3 ] );
			dstMaterial.m_Emissive		  = glm::vec4( emissiveFactor[ 0 ], emissiveFactor[ 1 ], emissiveFactor[ 2 ], 0.f ); // TODO: Put emissive strength here or pre-multiply it.
			dstMaterial.m_MetallicFactor  = float( pbrMetallicRoughness.metallicFactor );
			dstMaterial.m_RoughnessFactor = float( pbrMetallicRoughness.roughnessFactor );
			dstMaterial.m_AlphaCutoff	  = float( srcMaterial.alphaCutoff );

			dstMaterial.m_AlbedoTexturePath			= "";
			dstMaterial.m_NormalsTexturePath		= "";
			dstMaterial.m_MetalRoughnessTexturePath = "";
			dstMaterial.m_EmissiveTexturePath		= "";

			if ( pbrMetallicRoughness.baseColorTexture.index > -1 ) {
				const auto& texture = model.textures[ pbrMetallicRoughness.baseColorTexture.index ];
				dstMaterial.m_AlbedoTexturePath = m_BaseDir + model.images[texture.source].uri;
			}

			const auto& images = model.images;
			const auto& textures = model.textures;

			if ( const auto index = pbrMetallicRoughness.metallicRoughnessTexture.index; index > -1 )
				dstMaterial.m_MetalRoughnessTexturePath = m_BaseDir + images[ textures[ index ].source ].uri;
			if ( const auto index = srcMaterial.normalTexture.index; index > -1 )
				dstMaterial.m_NormalsTexturePath = m_BaseDir + images[ textures[ index ].source ].uri;
			if ( const auto index = srcMaterial.emissiveTexture.index; index > -1 )
				dstMaterial.m_EmissiveTexturePath = m_BaseDir + images[ textures[ index ].source ].uri;

			m_Materials.push_back( matEntity );
		}
	}

	void GLTFImporter::LoadAnimations( const tinygltf::Model& model ) {
		auto& registry = m_Scene.GetRegistry();

		for ( size_t i = 0; i < model.animations.size(); i++ ) {
			const auto& gltfAnimation = model.animations[ i ];

			entt::entity animationEntity = registry.create();
			Animation& animation = registry.emplace<Animation>( animationEntity, Animation( model, gltfAnimation ) );
			registry.emplace<EntityTag>( animationEntity, gltfAnimation.name.empty() ? "Animation" : gltfAnimation.name );
			
			m_Animations.push_back( animationEntity );
		}
	}

	void GLTFImporter::LoadNode( entt::entity parent, const tinygltf::Model& model, const tinygltf::Node& node, glm::mat4 parentTransform ) {
		auto& registry = m_Scene.GetRegistry();

		glm::mat4 localTransform(1.f);
		bool hasTransform = ( node.matrix.size() == 16 ) || 
			( node.translation.size() == 3 ) || 
			( node.rotation.size() == 4) || 
			( node.scale.size() == 3);

		if( node.matrix.size() == 16) {
			localTransform = glm::make_mat4(node.matrix.data());
		} else {
			glm::vec3 T = {0.f, 0.f, 0.f};
			glm::quat R = {1.f, 0.f, 0.f, 0.f};
			glm::vec3 S = {1.f, 1.f, 1.f};

			if ( node.translation.size() == 3 ) {
				T = glm::vec3( float( node.translation[ 0 ] ), float( node.translation[ 1 ] ), float( node.translation[ 2 ] ) );
			} if ( node.rotation.size() == 4 ) {
				R = glm::quat( float(node.rotation[ 3 ] ), float( node.rotation[ 0 ] ), float( node.rotation[ 1 ] ), float( node.rotation[ 2 ] ) );
			} if ( node.scale.size() == 3 ) {
				S = glm::vec3( float( node.scale[ 0 ] ), float( node.scale[ 1 ] ), float( node.scale[ 2 ] ) );
			}

			localTransform = glm::translate( glm::mat4( 1.f ), T ) * glm::mat4_cast( R ) * glm::scale( glm::mat4( 1.f ), S );
		}

		parentTransform *= localTransform;

		entt::entity entity = parent;

		// Flatten entity hierarchy.
		if ( hasTransform || ( node.mesh && registry.all_of<Mesh>( parent ) ) ) {
			std::string name = node.name.empty() ? "GLTF Node" : node.name;
			entity = m_Scene.CreateEntityWithTransform( name );

			if ( parent != entt::null )
				m_Scene.ParentTo( entity, parent );
		}

		// TODO: Verify these are correct.
		if( hasTransform ) {
			Transform& transform = registry.get<Transform>(entity);
			transform.m_LocalTransform = parentTransform;
		}

		if( node.mesh > -1 ) {
			const auto& mesh = model.meshes[ node.mesh ];

			std::string tag = registry.all_of<EntityTag>( entity ) ? registry.get<EntityTag>( entity ).m_Name : "Mesh";

			if( mesh.primitives.size() == 1) {
				LoadMesh( entity, model, node, mesh.primitives[0]);
			} else if( mesh.primitives.size() > 1 ) {
				for( auto i = 0; i < mesh.primitives.size(); i++ ) {
					entt::entity subEntity = m_Scene.CreateEntityWithTransform();
					LoadMesh( subEntity, model, node, mesh.primitives[i]);

					EntityTag& sub_name = registry.get<EntityTag>( subEntity );
					sub_name.m_Name = tag + "-" + std::to_string( i );

					m_Scene.ParentTo( subEntity, entity );	
				}
			}
		}

		for ( int child : node.children ) {
			LoadNode( entity, model, model.nodes[ child ], parentTransform );
		}
	}

	void GLTFImporter::LoadSkin( const tinygltf::Model& model, const tinygltf::Skin& skin ) {
		auto& registry = m_Scene.GetRegistry();

		entt::entity entity = registry.create();

		Skeleton& skeleton = registry.emplace<Skeleton>( entity );

		if ( skin.inverseBindMatrices > -1 ) {
			const auto& accessor = model.accessors[ skin.inverseBindMatrices ];
			const auto& bufferView = model.bufferViews[ accessor.bufferView ];
			const auto& buffer = model.buffers[ bufferView.buffer ];
			const float* ptr = reinterpret_cast< const float* >( &( buffer.data[ accessor.byteOffset + bufferView.byteOffset ] ) );

			skeleton.m_InverseBindMatrices.resize( accessor.count, glm::mat4( 1.f ) );
			std::memcpy( skeleton.m_InverseBindMatrices.data(), ptr, accessor.count * sizeof( glm::mat4 ) );
		}

		skeleton.m_BoneTransformMatrices.resize( skeleton.m_InverseBindMatrices.size(), glm::mat4( 1.0f ) );
		skeleton.m_BoneWSTransformMatrices.resize( skeleton.m_InverseBindMatrices.size(), glm::mat4( 1.0f ) );

		// Index 0 should work fine when skeleton is missing.
		int rootNode = skin.skeleton != -1 ? skin.skeleton : skin.joints[ 0 ];

		skeleton.m_RootBone.m_Index = GetJointIndexForNode( skin, rootNode );
		skeleton.m_RootBone.m_Name = model.nodes[ rootNode ].name;

		CopyJointHierarchy( model, skin, model.nodes[ rootNode ], skeleton.m_RootBone );

		m_Skins.push_back( entity );
	}

	int GLTFImporter::GetJointIndexForNode( const tinygltf::Skin& skin, const int node ) {
		for ( size_t i = 0; i < skin.joints.size(); i++ ) {
			if ( skin.joints[ i ] == node )
				return int( i );
		}

		return -1;
	}

	void GLTFImporter::CopyJointHierarchy( const tinygltf::Model& model, const tinygltf::Skin& skin, const tinygltf::Node& node, Bone& bone ) {
		for ( size_t i = 0; i < node.children.size(); i++ ) {
			const int nodeIndex = node.children[ i ];
			const int jointIndex = GetJointIndexForNode( skin, nodeIndex );
			const auto& childNode = model.nodes[ nodeIndex ];

			if ( jointIndex != -1 ) {
				Bone& childBone = bone.m_Children.emplace_back();
				childBone.m_Index = jointIndex;
				childBone.m_Name = childNode.name;

				CopyJointHierarchy( model, skin, childNode, childBone );
			}
		}
	}
}