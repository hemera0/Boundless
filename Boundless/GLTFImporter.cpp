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

		bool isBinary = std::filesystem::path( path ).extension() == ".glb";
		bool result = false;

		if( isBinary ){
			result = loader.LoadBinaryFromFile(&model, &err, &warn, path );
		} else {
			result = loader.LoadASCIIFromFile(&model, &err, &warn, path);
		}

		auto& registry = m_Scene.GetRegistry();

		entt::entity rootEntity = registry.create();

		LoadMaterials( rootEntity, model );
		LoadAnimations( rootEntity, model );

		const auto& scene = model.scenes[model.defaultScene == -1 ? 0 : model.defaultScene ];

		glm::mat4 rootTransform(1.f);

		for(auto i = 0; i < scene.nodes.size(); i++) {
			LoadNode( rootEntity, model, model.nodes[scene.nodes[i]], rootTransform );
		}

		if(!result)
			return false;

		return true;
	}

	void GLTFImporter::LoadMesh( entt::entity entity, const tinygltf::Model& model, const tinygltf::Primitive& primitive ) { 
		auto& registry = m_Scene.GetRegistry();
		auto& mesh = registry.emplace<Mesh>(entity);
	
		mesh.m_Material = primitive.material;

		for(const auto& [type, index] :  primitive.attributes) {
			if (index < 0)
				continue;

			const auto& accessor = model.accessors[ index ];
			const auto& bufferView = model.bufferViews[ accessor.bufferView ];
			const float* buffer = reinterpret_cast< const float* >( &( model.buffers[ bufferView.buffer ].data[ accessor.byteOffset + bufferView.byteOffset ] ) );

			for( auto i = 0; i < accessor.count; i++ ) {
				if ( type == "POSITION" ) {
					mesh.m_Positions.push_back( glm::vec3( buffer[ 0 ], buffer[ 1 ], buffer[ 2 ] ) );
				} if ( type == "NORMAL" ) {
					mesh.m_Normals.push_back( glm::vec3( buffer[ 0 ], buffer[ 1 ], buffer[ 2 ] ) );
				} if ( type == "TEXCOORD_0" ) {
					mesh.m_Texcoords.push_back( glm::vec2( buffer[ 0 ], buffer[ 1 ] ) );
				}

				buffer += tinygltf::GetNumComponentsInType(accessor.type);
			}
		}

		if( primitive.indices > -1 ) {
			const auto& accessor = model.accessors[ primitive.indices ];
			const auto& bufferView = model.bufferViews[ accessor.bufferView ];
			const uint32_t* dataPtr = reinterpret_cast< const uint32_t* >( &( model.buffers[ bufferView.buffer ].data[ accessor.byteOffset + bufferView.byteOffset ] ) );

			for ( uint32_t i = 0; i < accessor.count; i++ ) {
				if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT ) {
					mesh.m_Indices.push_back( dataPtr[ i ] );
				}

				if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT ) {
					const uint16_t* buf = reinterpret_cast< const uint16_t* >( dataPtr );
					mesh.m_Indices.push_back( buf[ i ] );
				}

				if ( accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE ) {
					const uint8_t* buf = reinterpret_cast< const uint8_t* >( dataPtr );
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

			dstMaterial.m_Albedo = glm::vec4( baseColorFactor[ 0 ], baseColorFactor[ 1 ], baseColorFactor[ 2 ], baseColorFactor[ 3 ] );
			dstMaterial.m_Emissive = glm::vec4( emissiveFactor[ 0 ], emissiveFactor[ 1 ], emissiveFactor[ 2 ], 0.f );

			if ( srcMaterial.extensions.find( "KHR_materials_emissive_strength" ) != srcMaterial.extensions.cend() ) {
				float emissiveStrength = static_cast<float>(srcMaterial.extensions.at( "KHR_materials_emissive_strength" ).Get( "emissiveStrength" ).GetNumberAsDouble());
				dstMaterial.m_Emissive *= emissiveStrength;
			}

			dstMaterial.m_MetallicFactor = static_cast< float >( pbrMetallicRoughness.metallicFactor );
			dstMaterial.m_RoughnessFactor = static_cast< float >( pbrMetallicRoughness.roughnessFactor );

			dstMaterial.m_AlphaCutoff = static_cast< float >( srcMaterial.alphaCutoff );

			dstMaterial.m_AlbedoTexturePath = "";
			dstMaterial.m_NormalsTexturePath = "";
			dstMaterial.m_MetalRoughnessTexturePath = "";
			dstMaterial.m_EmissiveTexturePath = "";

			// TODO: Better texture loading
			if ( pbrMetallicRoughness.baseColorTexture.index > -1 ) {
				const auto& texture = model.textures[ pbrMetallicRoughness.baseColorTexture.index ];
				
				if(texture.sampler > -1) {
					auto& sampler = model.samplers[ texture.sampler ];

					const auto GetFilterType = [] (int filter) -> VkFilter {
						switch(filter) {
						case TINYGLTF_TEXTURE_FILTER_NEAREST:
							return VK_FILTER_NEAREST;
						case TINYGLTF_TEXTURE_FILTER_LINEAR:
							return VK_FILTER_LINEAR;
						}

						return VK_FILTER_LINEAR;
					};
				
					const auto GetRepeatType = [](int wrap) -> VkSamplerAddressMode {
						switch(wrap) {
						case TINYGLTF_TEXTURE_WRAP_REPEAT:
							return VK_SAMPLER_ADDRESS_MODE_REPEAT;
						case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
							return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
						case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
							return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
						}

						return VK_SAMPLER_ADDRESS_MODE_REPEAT;
					};

					dstMaterial.m_AlbedoSampler = {
						.m_MinFilter = GetFilterType( sampler.minFilter ),
						.m_MagFilter = GetFilterType( sampler.magFilter ),
						.m_WrapS = GetRepeatType( sampler.wrapS ),
						.m_WrapT = GetRepeatType( sampler.wrapT )
					};
				} else {
					dstMaterial.m_AlbedoSampler = {
						.m_MinFilter = VK_FILTER_LINEAR,
						.m_MagFilter = VK_FILTER_LINEAR,
						.m_WrapS = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
						.m_WrapT = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
					};
				}

				dstMaterial.m_AlbedoTexturePath = m_BaseDir + model.images[texture.source].uri;
			}

			if ( pbrMetallicRoughness.metallicRoughnessTexture.index > -1 )
				dstMaterial.m_MetalRoughnessTexturePath = m_BaseDir + model.images[ model.textures[ pbrMetallicRoughness.metallicRoughnessTexture.index ].source ].uri;
			if ( srcMaterial.normalTexture.index > -1 )
				dstMaterial.m_NormalsTexturePath = m_BaseDir + model.images[model.textures[srcMaterial.normalTexture.index].source].uri;
			if ( srcMaterial.emissiveTexture.index > -1 )
				dstMaterial.m_EmissiveTexturePath = m_BaseDir + model.images[model.textures[srcMaterial.emissiveTexture.index].source].uri;

			m_Materials.push_back(matEntity);
		}
	}

	void GLTFImporter::LoadAnimations( entt::entity entity, const tinygltf::Model& model ) {

	}

	void GLTFImporter::LoadNode( entt::entity parent, const tinygltf::Model& model, const tinygltf::Node& node, glm::mat4 nodeTransform ) {
		auto& registry = m_Scene.GetRegistry();

		entt::entity nodeEntity = registry.create();

		glm::mat4 localTransform(1.f);

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

		nodeTransform *= localTransform;

		// TODO: Transform components and parenting...
		auto& transform = registry.emplace<Transform>(nodeEntity);

		transform.m_WorldTransform = nodeTransform;

		if( node.mesh > -1 ) {
			const auto& mesh = model.meshes[ node.mesh ];

			if( mesh.primitives.size() == 1) {
				LoadMesh(nodeEntity, model, mesh.primitives[0]);
			} else if( mesh.primitives.size() > 1 ) {
				for( auto i = 0; i < mesh.primitives.size(); i++ ) {
					entt::entity submeshEntity = registry.create();
					LoadMesh(submeshEntity, model, mesh.primitives[i]);
				}
			}
		}

		for(auto i = 0; i < node.children.size(); i++) {
			LoadNode(nodeEntity, model, model.nodes[node.children[i]], nodeTransform);
		}
	}
}