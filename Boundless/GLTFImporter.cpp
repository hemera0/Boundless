#include "Pch.hpp"
#include "GLTFImporter.hpp"
#include "Transform.hpp"

namespace Boundless {
	GLTFImporter::GLTFImporter( Scene& inScene ) : m_Scene(inScene) {}

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

		entt::entity root = m_Scene.GetRootEntity();

		LoadMaterials( root, model );
		LoadAnimations( root, model );

		const auto& scene = model.scenes[model.defaultScene == -1 ? 0 : model.defaultScene ];
		for(auto i = 0; i < scene.nodes.size(); i++) {
			LoadNode( root, model, model.nodes[scene.nodes[i]] );
		}

		return true;
	}

	void GLTFImporter::LoadMesh( entt::entity entity, const tinygltf::Model& model, const tinygltf::Primitive& primitive ) { 
		auto& registry = m_Scene.GetRegistry();
		auto& mesh = registry.emplace<Mesh>(entity);
	
		mesh.m_Material = primitive.material;

		for(const auto& [type, index] :  primitive.attributes) {
			if (index < 0)
				continue;

			const auto& accessor	= model.accessors[ index ];
			const auto& bufferView	= model.bufferViews[ accessor.bufferView ];
			const float* buffer		= reinterpret_cast< const float* >( &( model.buffers[ bufferView.buffer ].data[ accessor.byteOffset + bufferView.byteOffset ] ) );

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

		if( primitive.indices > -1 ) {
			const auto& accessor	= model.accessors[ primitive.indices ];
			const auto& bufferView	= model.bufferViews[ accessor.bufferView ];
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

	void GLTFImporter::LoadMaterials( entt::entity entity, const tinygltf::Model& model ) {
		auto& registry = m_Scene.GetRegistry();

		for(auto i = 0 ; i < model.materials.size(); i++) {
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
			dstMaterial.m_Emissive		  = glm::vec4( emissiveFactor[ 0 ], emissiveFactor[ 1 ], emissiveFactor[ 2 ], 0.f );
			dstMaterial.m_MetallicFactor  = static_cast< float >( pbrMetallicRoughness.metallicFactor );
			dstMaterial.m_RoughnessFactor = static_cast< float >( pbrMetallicRoughness.roughnessFactor );
			dstMaterial.m_AlphaCutoff	  = static_cast< float >( srcMaterial.alphaCutoff );

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

			m_Materials.push_back(matEntity);
		}
	}

	void GLTFImporter::LoadAnimations( entt::entity entity, const tinygltf::Model& model ) {

	}

	void GLTFImporter::LoadNode( entt::entity parent, const tinygltf::Model& model, const tinygltf::Node& node ) {
		auto& registry = m_Scene.GetRegistry();

		glm::mat4 localTransform(1.f);
		bool hasTransform = ( node.matrix.size() == 16 ) || 
			( node.translation.size() == 3 ) || 
			( node.rotation.size() == 4) || 
			( node.scale.size() == 3);

		if( node.matrix.size() == 16) {
			localTransform = glm::make_mat4(node.matrix.data());
		} else {
			if ( node.translation.size() == 3 ) {
				localTransform = glm::translate(localTransform, glm::vec3( float( node.translation[ 0 ] ), float( node.translation[ 1 ] ), float( node.translation[ 2 ] ) ));
			} if ( node.rotation.size() == 4 ) {
				localTransform = localTransform * glm::mat4( glm::quat( float(node.rotation[ 3 ] ), float( node.rotation[ 0 ] ), float( node.rotation[ 1 ] ), float( node.rotation[ 2 ] ) ) );
			} if ( node.scale.size() == 3 ) {
				localTransform = glm::scale( localTransform, glm::vec3( float( node.scale[ 0 ] ), float( node.scale[ 1 ] ), float( node.scale[ 2 ] ) ) );
			}
		}

		entt::entity entity = parent;
		if( hasTransform || ( node.mesh > -1 && registry.all_of<Mesh>( parent ) ) ) {
			entity = m_Scene.CreateGameObject();
			m_Scene.ParentTo( entity, parent );
		}

		if(hasTransform) {
			Transform& transform = registry.get<Transform>(entity);
			transform.m_LocalTransform = localTransform;
		}

		if( node.mesh > -1 ) {
			const auto& mesh = model.meshes[ node.mesh ];

			if( mesh.primitives.size() == 1) {
				LoadMesh( entity, model, mesh.primitives[0]);
			} else if( mesh.primitives.size() > 1 ) {
				for( auto i = 0; i < mesh.primitives.size(); i++ ) {
					entt::entity subEntity = m_Scene.CreateGameObject();
					
					LoadMesh( subEntity, model, mesh.primitives[i]);
					m_Scene.ParentTo( subEntity, entity );
				}
			}
		}

		for(auto i = 0; i < node.children.size(); i++) {
			LoadNode(entity, model, model.nodes[node.children[i]]);
		}
	}
}