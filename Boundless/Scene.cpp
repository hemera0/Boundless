#include "Pch.hpp"
#include "Scene.hpp"
#include "Engine.hpp"

namespace Boundless {
	Scene::Scene() { 
		m_RootEntity = m_Registry.create();
		m_Registry.emplace<Transform>( m_RootEntity );
		m_Registry.emplace<EntityRelation>( m_RootEntity );
		m_Registry.emplace<EntityTag>( m_RootEntity, "Root Entity" );
	}

	void Scene::UploadToGPU( Device& device ) { 
		UploadMeshes( device );
		UploadTLAS( device );
		UploadTextures( device );
		UploadMaterials( device );
		UploadSkeletons( device );
	}
	
	void Scene::UploadMeshes( Device& device ) { 
		for ( entt::entity entity : m_Registry.view<Mesh>() ) {
			Mesh& mesh = m_Registry.get<Mesh>( entity );

			if ( mesh.m_VertexBuffer == BufferHandle::Invalid ) {
				mesh.m_VertexBuffer = device.CreateBuffer(
					Buffer::Desc{
						.m_Size = mesh.m_Vertices.size() * sizeof( MeshVertexData ),
						.m_Usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);
			}

			if ( mesh.m_IndexBuffer == BufferHandle::Invalid && !mesh.m_Indices.empty() ) {
				mesh.m_IndexBuffer = device.CreateBuffer(
					Buffer::Desc{
						.m_Size = mesh.m_Indices.size() * sizeof( uint32_t ),
						.m_Usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);
			}

			if ( !mesh.m_BoneIndices.empty() ) {
				mesh.m_BoneIndexBuffer = device.CreateBuffer( Buffer::Desc{
						.m_Size = uint32_t( mesh.m_BoneIndices.size() * sizeof( glm::uvec4 ) ),
						.m_Usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					} 
				);

				Buffer& boneIndexBuffer = device.GetBuffer( mesh.m_BoneIndexBuffer );

				size_t bufferSize = mesh.m_BoneIndices.size() * sizeof( glm::uvec4 );
				std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_BoneIndices.data(), bufferSize );

				// TOOD: fix mem leak.
				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
				commandBuffer.CopyBuffer( *stagingBuffer, boneIndexBuffer, bufferSize );
				commandBuffer.End();
				commandBuffer.Submit( device.GetQueue() );
			}

			if ( !mesh.m_BoneWeights.empty() ) {
				mesh.m_BoneWeightBuffer = device.CreateBuffer( Buffer::Desc{
						.m_Size = uint32_t( mesh.m_BoneWeights.size() * sizeof( glm::vec4 ) ),
						.m_Usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					} 
				);

				Buffer& boneWeightsBuffer = device.GetBuffer( mesh.m_BoneWeightBuffer );

				size_t bufferSize = mesh.m_BoneWeights.size() * sizeof( glm::vec4 );
				std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_BoneWeights.data(), bufferSize );

				// TOOD: fix mem leak.
				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
				commandBuffer.CopyBuffer( *stagingBuffer, boneWeightsBuffer, bufferSize );
				commandBuffer.End();
				commandBuffer.Submit( device.GetQueue() );
			}

			if ( !mesh.m_BoneWeights.empty() && !mesh.m_BoneIndices.empty() ) {
				mesh.m_SkinnedVertexBuffer = device.CreateBuffer( Buffer::Desc{
						.m_Size = mesh.m_Vertices.size() * sizeof( MeshVertexData ),
						.m_Usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					} 
				);
			}

			if ( !mesh.m_Indices.empty() ) {
				Buffer& indexBuffer = device.GetBuffer( mesh.m_IndexBuffer );

				size_t bufferSize = mesh.m_Indices.size() * sizeof( uint32_t );
				std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_Indices.data(), bufferSize );

				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
				commandBuffer.CopyBuffer( *stagingBuffer, indexBuffer, bufferSize );
				commandBuffer.End();
				commandBuffer.Submit( device.GetQueue() );
			}

			{
				Buffer& vertexBuffer = device.GetBuffer( mesh.m_VertexBuffer );

				size_t bufferSize = mesh.m_Vertices.size() * sizeof( MeshVertexData );
				std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_Vertices.data(), bufferSize );

				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
				commandBuffer.CopyBuffer( *stagingBuffer, vertexBuffer, bufferSize );
				commandBuffer.End();
				commandBuffer.Submit( device.GetQueue() );
			}

			// Build RT BLAS.
			if(mesh.m_VertexBuffer != BufferHandle::Invalid && mesh.m_IndexBuffer != BufferHandle::Invalid)
			{
				vk::AccelerationStructureGeometryTrianglesDataKHR triangles = {};
				triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
				triangles.vertexData.deviceAddress = device.GetBuffer( mesh.m_VertexBuffer ).GetDeviceAddress();
				triangles.vertexStride = sizeof( MeshVertexData );
				triangles.maxVertex = uint32_t( mesh.m_Positions.size() - 1 );
				triangles.indexType = vk::IndexType::eUint32;
				triangles.indexData.deviceAddress = device.GetBuffer( mesh.m_IndexBuffer ).GetDeviceAddress();

				vk::AccelerationStructureGeometryKHR accelerationStructureGeo = {};
				accelerationStructureGeo.geometryType = vk::GeometryTypeKHR::eTriangles;
				accelerationStructureGeo.geometry.triangles = triangles;

				vk::AccelerationStructureBuildGeometryInfoKHR buildInfo = {};
				buildInfo.setType( vk::AccelerationStructureTypeKHR::eBottomLevel )
					.setFlags( vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate )
					.setMode( vk::BuildAccelerationStructureModeKHR::eBuild )
					.setGeometries( { accelerationStructureGeo } );

				uint32_t maxPrimitiveCounts = uint32_t( mesh.m_Indices.size() ) / 3;


				vk::AccelerationStructureBuildSizesInfoKHR sizeInfo 
					= device->getAccelerationStructureBuildSizesKHR( vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, { maxPrimitiveCounts } );

				mesh.m_BlasBuffer = device.CreateBuffer(
					Buffer::Desc{
						.m_Size = sizeInfo.accelerationStructureSize,
						.m_Usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);

				BufferHandle scratchBuffer = device.CreateBuffer( Buffer::Desc {
						.m_Size = sizeInfo.buildScratchSize,
						.m_Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					} 
				);

				vk::AccelerationStructureCreateInfoKHR accelerationInfo = {};
				accelerationInfo.buffer = device.GetBuffer( mesh.m_BlasBuffer ).GetHandle();
				accelerationInfo.offset = 0;
				accelerationInfo.size = sizeInfo.accelerationStructureSize;
				accelerationInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

				mesh.m_Blas = device->createAccelerationStructureKHR( accelerationInfo );

				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );

				{
					buildInfo.scratchData.deviceAddress = device.GetBuffer( scratchBuffer ).GetDeviceAddress();
					buildInfo.dstAccelerationStructure = mesh.m_Blas;

					vk::AccelerationStructureBuildRangeInfoKHR buildRange = {};
					buildRange.primitiveCount = maxPrimitiveCounts;

					commandBuffer->buildAccelerationStructuresKHR( { buildInfo }, { &buildRange } );

					vk::AccessFlags2 accessFlags = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
					commandBuffer.StageBarrier( vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, accessFlags, vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, accessFlags );
				}
				
				commandBuffer.End();
				commandBuffer.Submit( device.GetQueue() );
				
				// TODO: fixme (device->ReleaseBuffer or something)
				// Wow this looks ghetto (Technically this is because it wastes a slot in the vector now with garbage data...
				// delete device->GetBuffer(scratchBuffer);
			}
		}
	}

	// TODO: Update blas for animations & Fix the transforms.
	void Scene::UploadTLAS( Device& device ) {
		uint32_t totalPrimitiveCount = 0;

		std::vector<vk::AccelerationStructureInstanceKHR> RTinstances = {};
		for ( entt::entity entity : m_Registry.view<Mesh>() ) {
			Mesh& mesh = m_Registry.get<Mesh>( entity );
			if(mesh.m_BlasBuffer == BufferHandle::Invalid || mesh.m_Blas == VK_NULL_HANDLE)
				continue;

			vk::AccelerationStructureInstanceKHR instance = {};

			// TODO: FILL OUT ALL OF THIS...
			// memcpy( instance.transform.matrix[ 0 ], &xform[ 0 ], sizeof( float ) * 3 );
			// memcpy( instance.transform.matrix[ 1 ], &xform[ 1 ], sizeof( float ) * 3 );
			// memcpy( instance.transform.matrix[ 2 ], &xform[ 2 ], sizeof( float ) * 3 );
			// instance.transform.matrix[ 0 ][ 3 ] = draw.position.x;
			// instance.transform.matrix[ 1 ][ 3 ] = draw.position.y;
			// instance.transform.matrix[ 2 ][ 3 ] = draw.position.z;
			
			instance.transform.matrix[ 0 ][ 0 ] = 1.f;
			instance.transform.matrix[ 1 ][ 1 ] = 1.f;
			instance.transform.matrix[ 2 ][ 2 ] = 1.f;

			// TODO: Find a way to map to an entity index...
			// instance.instanceCustomIndex = ;
			instance.mask = 0xFF;
			instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
			instance.accelerationStructureReference = device->getAccelerationStructureAddressKHR( vk::AccelerationStructureDeviceAddressInfoKHR{ mesh.m_Blas } );

			RTinstances.push_back(instance);

			totalPrimitiveCount += uint32_t( mesh.m_Indices.size() ) / 3;
		}

		m_TLASInstances = device.CreateBuffer(
			Buffer::Desc {
				.m_Size = sizeof( vk::AccelerationStructureInstanceKHR ) * RTinstances.size(),
				.m_Usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				.m_Mappable = true
			}
		);

		Buffer& tlasInstanceBuffer = device.GetBuffer( m_TLASInstances );
		tlasInstanceBuffer.Patch( RTinstances.data(), sizeof( vk::AccelerationStructureInstanceKHR ) * RTinstances.size() );
		
		if(m_TLAS == VK_NULL_HANDLE) {
			vk::AccelerationStructureGeometryInstancesDataKHR instances = { {}, tlasInstanceBuffer.GetDeviceAddress() };
			vk::AccelerationStructureGeometryKHR geometry = {};
			geometry.geometryType = vk::GeometryTypeKHR::eInstances;
			geometry.geometry.instances = instances;

			vk::AccelerationStructureBuildGeometryInfoKHR buildInfo = {};
			buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
			buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
			buildInfo.geometryCount = 1;
			buildInfo.pGeometries = &geometry;

			vk::AccelerationStructureBuildSizesInfoKHR sizeInfo 
				= device->getAccelerationStructureBuildSizesKHR( vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, { totalPrimitiveCount } );

			m_TLASBuffer = device.CreateBuffer(
				Buffer::Desc{
					.m_Size = sizeInfo.accelerationStructureSize,
					.m_Usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
					.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
				});

			m_TLASScratchBuffer = device.CreateBuffer(
				Buffer::Desc{
					.m_Size = std::max( sizeInfo.buildScratchSize, sizeInfo.updateScratchSize ),
					.m_Usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
				});

			vk::AccelerationStructureCreateInfoKHR accelerationInfo = {};
			accelerationInfo.buffer = device.GetBuffer(m_TLASBuffer);
			accelerationInfo.size = sizeInfo.accelerationStructureSize;
			accelerationInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;

			m_TLAS = device->createAccelerationStructureKHR( accelerationInfo );
		}
	}

	void Scene::BuildTLAS( Device& device, CommandBuffer& commandBuffer ) {
		Buffer& tlasInstanceBuffer = device.GetBuffer( m_TLASInstances );
		Buffer& tlasScratchBuffer = device.GetBuffer( m_TLASScratchBuffer );

		vk::AccelerationStructureGeometryInstancesDataKHR instances = { {}, tlasInstanceBuffer.GetDeviceAddress() };
		vk::AccelerationStructureGeometryKHR geometry = {};
		geometry.geometryType = vk::GeometryTypeKHR::eInstances;
		geometry.geometry.instances = instances;
		
		vk::AccelerationStructureBuildGeometryInfoKHR buildInfo = {};
		buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
		buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;
		buildInfo.srcAccelerationStructure = m_TLAS;
		buildInfo.dstAccelerationStructure = m_TLAS;
		buildInfo.scratchData.deviceAddress = tlasScratchBuffer.GetDeviceAddress();

		uint32_t tlasInstancesCount = uint32_t( tlasInstanceBuffer.GetSize() ) / sizeof( vk::AccelerationStructureInstanceKHR );
		vk::AccelerationStructureBuildRangeInfoKHR buildRange = { tlasInstancesCount };
		
		commandBuffer->buildAccelerationStructuresKHR( { buildInfo }, { &buildRange });
		
		vk::AccessFlags2 accessFlags = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
		commandBuffer.StageBarrier( vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, accessFlags, vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader, accessFlags );
		
		vk::WriteDescriptorSetAccelerationStructureKHR asInfo = {};
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &m_TLAS;
		
		vk::WriteDescriptorSet write = {};
		write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		write.pNext = &asInfo;
		write.dstBinding = 1;
		write.dstSet = device.GetGlobalResources();
		write.descriptorCount = 1;
		write.dstArrayElement = 0;

		device->updateDescriptorSets( write, {} );
	}
	
	void Scene::UploadTextures( Device& device ) { 
		for ( auto ent : m_Registry.view<Material>() ) {
			Material& mat = m_Registry.get<Material>( ent );

			if ( !mat.m_AlbedoTexturePath.empty() ) {
				mat.m_AlbedoTexture = device.LoadImageFromFile( mat.m_AlbedoTexturePath, true ).first;
			}

			if ( !mat.m_NormalsTexturePath.empty() ) {
				mat.m_NormalsTexture = device.LoadImageFromFile( mat.m_NormalsTexturePath, false ).first;
			}

			if ( !mat.m_MetalRoughnessTexturePath.empty() ) {
				mat.m_MetalRoughnessTexture = device.LoadImageFromFile( mat.m_MetalRoughnessTexturePath, false ).first;
			}

			if ( !mat.m_EmissiveTexturePath.empty() ) {
				mat.m_EmissiveTexture = device.LoadImageFromFile( mat.m_EmissiveTexturePath, false ).first;
			}
		}
	}
	
	void Scene::UploadMaterials( Device& device ) { 
		std::vector<GPUMaterial> materials{};

		for ( auto ent : m_Registry.view<Material>() ) {
			Material& mat = m_Registry.get<Material>( ent );

			GPUMaterial gpuMat = {};
			gpuMat.m_Index				   = mat.m_Index;
			gpuMat.m_MetallicFactor		   = mat.m_MetallicFactor;
			gpuMat.m_RoughnessFactor	   = mat.m_RoughnessFactor;
			gpuMat.m_AlphaMode			   = mat.m_AlphaMode;
			gpuMat.m_AlphaCutoff		   = mat.m_AlphaCutoff;
			gpuMat.m_AlbedoTexture         = mat.m_AlbedoTexture;
			gpuMat.m_NormalsTexture		   = mat.m_NormalsTexture;
			gpuMat.m_MetalRoughnessTexture = mat.m_MetalRoughnessTexture;
			gpuMat.m_EmissiveTexture	   = mat.m_EmissiveTexture;
			gpuMat.m_Albedo				   = mat.m_Albedo;
			gpuMat.m_Emissive			   = mat.m_Emissive;

			materials.push_back( gpuMat );
		}

		// TODO: This really needs to be fixed. Materials are breaking when loading more than 1 gltf file.
		// TODO: Fix ghettoness of this (It's not terrible since it will only happen on scene load basically but.
		std::sort( materials.begin(), materials.end(), [ & ]( const auto& a, const auto& b ) -> bool
		{
			return a.m_Index < b.m_Index;
		} );

		size_t bufferSize = materials.size() * sizeof( GPUMaterial );

		std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( bufferSize );

		m_MaterialBuffer = device.CreateBuffer(
			Buffer::Desc{
				.m_Size = bufferSize,
				.m_Usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
			}
		);

		stagingBuffer->Patch( materials.data(), materials.size() * sizeof( GPUMaterial ) );

		CommandBuffer commandBuffer = CommandBuffer( device );
		commandBuffer.Begin( vk::CommandBufferUsageFlagBits::eOneTimeSubmit );
		commandBuffer.CopyBuffer( *stagingBuffer, device.GetBuffer( m_MaterialBuffer ), bufferSize );
		commandBuffer.End();
		commandBuffer.Submit( device.GetQueue() );
	}

	void Scene::UploadSkeletons( Device& device ) { 
		auto view = m_Registry.view<Mesh>();
		for ( auto [entity, mesh] : view.each() ) {
			if ( !m_Registry.valid( mesh.m_Skeleton ) || !m_Registry.all_of<Skeleton>( mesh.m_Skeleton ) )
				continue;

			Skeleton& skeleton = m_Registry.get<Skeleton>( mesh.m_Skeleton );

			skeleton.m_BoneTransformMatrices.resize( skeleton.m_InverseBindMatrices.size(), glm::mat4( 1.0f ) );
			skeleton.m_BoneWSTransformMatrices.resize( skeleton.m_InverseBindMatrices.size(), glm::mat4( 1.0f ) );

			skeleton.m_BoneTransformsBuffer = device.CreateBuffer( Buffer::Desc{
					.m_Size = uint32_t( skeleton.m_BoneTransformMatrices.size() * sizeof( glm::mat4 ) ),
					.m_Usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
				} );
		}
	}

	void PrintEntityName( Scene& scene, entt::entity entity, std::string& tabs ) {
		auto& registry = scene.GetRegistry();

		const auto& relation = registry.get<EntityRelation>( entity );
		const auto& name = registry.get<EntityTag>( entity );

		printf( "%s%s\n", tabs.data(), name.m_Name.data() );

		for ( entt::entity child : relation.m_Children )
			PrintEntityName( scene, child, tabs );

		printf("-------------\n");

		tabs += ' ';
	}

	void PrintEntityHierarchy( Scene& scene, entt::entity entity, const std::string& tabs ) {
		auto& registry = scene.GetRegistry();

		std::string name = registry.all_of<EntityTag>(entity) ? registry.get<EntityTag>( entity ).m_Name : "Unnamed";
	
		printf("%s%s[%d]\n", tabs.data(), name.data(), int( entity ));

		const auto& relation = registry.get<EntityRelation>( entity );
		for ( entt::entity child : relation.m_Children ) {
			PrintEntityHierarchy( scene, child, tabs + " " );
		}
	}

	// TODO: Finish/fix some issues with this (Skeletal mesh doesn't match correct rest pose)
	void Scene::UpdateTransformsRecursive( entt::entity entity, const glm::mat4& parentTransform ) {
		const auto& relation = m_Registry.get<EntityRelation>( entity );
		
		glm::mat4 childTransform = parentTransform;

		if( Transform* transform = m_Registry.try_get<Transform>( entity ) ) {
			transform->m_WorldTransform = parentTransform * transform->m_LocalTransform;
			childTransform = transform->m_WorldTransform;
		}

		for ( entt::entity child : relation.m_Children )
			UpdateTransformsRecursive( child, childTransform );
	}

	void Scene::UpdateTransforms() { 
		UpdateTransformsRecursive(m_RootEntity, glm::mat4(1.f));
	}

	void Scene::UpdateAnimations( float deltaTime ) {
		entt::entity firstAnimation = entt::null;

		// TODO: Remove this
		static bool animate = false;
		if ( GetAsyncKeyState( VK_F2 ) & 1 )
			animate = !animate;

		for( entt::entity entity : m_Registry.view<Animation>() ) {
			Animation& animation = m_Registry.get<Animation>( entity );
			animation.Update( deltaTime );
		
			if ( firstAnimation == entt::null )
				firstAnimation = entity;
		}

		for ( entt::entity entity : m_Registry.view<Skeleton>() ) {
			Skeleton& skeleton = m_Registry.get<Skeleton>( entity );

			if ( animate )
				skeleton.m_Animation = firstAnimation;
			else
				skeleton.m_Animation = entt::null;

			if ( m_Registry.valid( skeleton.m_Animation ) && m_Registry.all_of<Animation>( skeleton.m_Animation ) ) {
				const Animation& animation = m_Registry.get<Animation>( skeleton.m_Animation );
				skeleton.UpdateFromAnimation( animation );
			}
		}
	}

	entt::entity Scene::GetParent( entt::entity entity ) {
		return m_Registry.get<EntityRelation>( entity ).m_Parent;
	}

	void Scene::ParentTo( entt::entity entity, entt::entity parent ) { 
		EntityRelation& rel = m_Registry.get<EntityRelation>(entity);
		if( rel.m_Parent == parent )
			return;

		if ( rel.m_Parent != entt::null ) {
			EntityRelation& oldParentRel = m_Registry.get<EntityRelation>( rel.m_Parent );
			auto& children = oldParentRel.m_Children;
			children.erase( std::remove( children.begin(), children.end(), entity ), children.end() );
		}

		rel.m_Parent = parent;

		auto& parentRel = m_Registry.get<EntityRelation>( parent );
		if ( std::find( parentRel.m_Children.begin(), parentRel.m_Children.end(), entity ) == parentRel.m_Children.end() ) {
			parentRel.m_Children.push_back( entity );
		}
	}

	entt::entity Scene::CreatedNamedEntity( const std::string& name ) {
		entt::entity ent = m_Registry.create();

		m_Registry.emplace<EntityRelation>( ent );
		m_Registry.emplace<EntityTag>( ent, name );

		ParentTo( ent, m_RootEntity );
		return ent;
	}

	entt::entity Scene::CreateEntityWithTransform( const std::string& name ) {
		entt::entity ent = m_Registry.create();

		m_Registry.emplace<Transform>( ent );
		m_Registry.emplace<EntityRelation>( ent );
		m_Registry.emplace<EntityTag>( ent, name );

		ParentTo( ent, m_RootEntity );
		
		return ent;
	}
}