#include "Pch.hpp"
#include "Scene.hpp"
#include "Engine.hpp"

namespace Boundless {
	Scene::Scene() { 
		m_RootEntity = m_Registry.create();
		m_Registry.emplace<Transform>(m_RootEntity);
		m_Registry.emplace<EntityRelation>(m_RootEntity);
	}

	void Scene::OnDeviceStart( Device& device ) { 
		m_UniformBuffer = device.CreateBuffer(
			Buffer::Desc{
				.m_Size = sizeof( FrameConstants ),
				.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO,
				.m_Mappable = true
			}
		);

		UploadMeshes( device );
		UploadTLAS( device );
		UploadTextures( device );
		UploadMaterials( device );
	}
	
	void Scene::UploadMeshes( Device& device ) { 
		for ( entt::entity entity : m_Registry.view<Mesh>() ) {
			Mesh& mesh = m_Registry.get<Mesh>( entity );

			if ( mesh.m_VertexBuffer == BufferHandle::Invalid ) {
				mesh.m_VertexBuffer = device.CreateBuffer(
					Buffer::Desc{
						.m_Size = mesh.m_Vertices.size() * sizeof( MeshVertexData ),
						.m_Usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);
			}

			if ( mesh.m_IndexBuffer == BufferHandle::Invalid && !mesh.m_Indices.empty() ) {
				mesh.m_IndexBuffer = device.CreateBuffer(
					Buffer::Desc{
						.m_Size = mesh.m_Indices.size() * sizeof( uint32_t ),
						.m_Usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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
				commandBuffer.Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
				commandBuffer.CopyBuffer( *stagingBuffer, indexBuffer, bufferSize );
				commandBuffer.End();
				commandBuffer.Submit( device.GetGraphicsQueue() );
			}

			{
				Buffer& vertexBuffer = device.GetBuffer( mesh.m_VertexBuffer );

				size_t bufferSize = mesh.m_Vertices.size() * sizeof( MeshVertexData );
				std::unique_ptr<StagingBuffer> stagingBuffer = device.CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_Vertices.data(), bufferSize );

				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
				commandBuffer.CopyBuffer( *stagingBuffer, vertexBuffer, bufferSize );
				commandBuffer.End();
				commandBuffer.Submit( device.GetGraphicsQueue() );
			}

			// Build RT BLAS.
			if(mesh.m_VertexBuffer != BufferHandle::Invalid && mesh.m_IndexBuffer != BufferHandle::Invalid)
			{
				VkAccelerationStructureGeometryKHR accelerationStructureGeo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
				accelerationStructureGeo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
				accelerationStructureGeo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				accelerationStructureGeo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				accelerationStructureGeo.geometry.triangles.vertexData.deviceAddress = device.GetBuffer(mesh.m_VertexBuffer).GetDeviceAddress();
				accelerationStructureGeo.geometry.triangles.vertexStride = sizeof( MeshVertexData );
				accelerationStructureGeo.geometry.triangles.maxVertex = uint32_t(mesh.m_Positions.size() - 1);
				accelerationStructureGeo.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
				accelerationStructureGeo.geometry.triangles.indexData.deviceAddress = device.GetBuffer(mesh.m_IndexBuffer).GetDeviceAddress();

				VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
				buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
				buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
				buildInfo.geometryCount = 1;
				buildInfo.pGeometries = &accelerationStructureGeo;

				uint32_t maxPrimitiveCounts = uint32_t( mesh.m_Indices.size() ) / 3;

				VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
				vkGetAccelerationStructureBuildSizesKHR( device.GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCounts, &sizeInfo );

				mesh.m_BlasBuffer = device.CreateBuffer(
					Buffer::Desc{
						.m_Size = sizeInfo.accelerationStructureSize,
						.m_Usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);

				BufferHandle scratchBuffer = device.CreateBuffer( Buffer::Desc {
						.m_Size = sizeInfo.buildScratchSize,
						.m_Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					} 
				);

				VkAccelerationStructureCreateInfoKHR accelerationInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
				accelerationInfo.buffer = device.GetBuffer( mesh.m_BlasBuffer ).GetHandle();
				accelerationInfo.offset = 0;
				accelerationInfo.size = sizeInfo.accelerationStructureSize;
				accelerationInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

				VkResult result = vkCreateAccelerationStructureKHR( device.GetDevice(), &accelerationInfo, nullptr, &mesh.m_Blas );
				assert(result == VK_SUCCESS);

				CommandBuffer commandBuffer = CommandBuffer( device );
				commandBuffer.Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

				{
					VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
					VkAccelerationStructureBuildRangeInfoKHR* buildRangePtrs[ 1 ] = { &buildRange };

					buildInfo.scratchData.deviceAddress = device.GetBuffer( scratchBuffer ).GetDeviceAddress();
					buildInfo.dstAccelerationStructure = mesh.m_Blas;
					buildRange.primitiveCount = maxPrimitiveCounts;

					vkCmdBuildAccelerationStructuresKHR( commandBuffer, 1, &buildInfo, buildRangePtrs );

					VkAccessFlags accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
					commandBuffer.StageBarrier( VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, accessFlags, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, accessFlags );
				}
				
				commandBuffer.End();
				commandBuffer.Submit( device.GetGraphicsQueue() );
				
				// TODO: fixme (device->ReleaseBuffer or something)
				// Wow this looks ghetto (Technically this is because it wastes a slot in the vector now with garbage data...
				// delete device->GetBuffer(scratchBuffer);
			}
		}
	}

	void Scene::UploadTLAS( Device& device ) {
		uint32_t totalPrimitiveCount = 0;

		std::vector<VkAccelerationStructureInstanceKHR> RTinstances = {};
		for ( entt::entity entity : m_Registry.view<Mesh>() ) {
			Mesh& mesh = m_Registry.get<Mesh>( entity );
			if(mesh.m_BlasBuffer == BufferHandle::Invalid || mesh.m_Blas == VK_NULL_HANDLE)
				continue;

			VkAccelerationStructureInstanceKHR instance = {};

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
			
			VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
			info.accelerationStructure = mesh.m_Blas;
			
			instance.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR( device.GetDevice(), &info );

			RTinstances.push_back(instance);

			totalPrimitiveCount += uint32_t( mesh.m_Indices.size() ) / 3;
		}

		m_TLASInstances = device.CreateBuffer(
			Buffer::Desc {
				.m_Size = sizeof( VkAccelerationStructureInstanceKHR ) * RTinstances.size(),
				.m_Usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				.m_Mappable = true
			}
		);

		Buffer& tlasInstanceBuffer = device.GetBuffer( m_TLASInstances );
		tlasInstanceBuffer.Patch( RTinstances.data(), sizeof( VkAccelerationStructureInstanceKHR ) * RTinstances.size() );
		
		if(m_TLAS == VK_NULL_HANDLE) {
			VkAccelerationStructureGeometryKHR geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
			geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
			geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
			geometry.geometry.instances.data.deviceAddress = tlasInstanceBuffer.GetDeviceAddress();

			VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
			buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
			buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildInfo.geometryCount = 1;
			buildInfo.pGeometries = &geometry;

			VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
			vkGetAccelerationStructureBuildSizesKHR( device.GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &totalPrimitiveCount, &sizeInfo );

			m_TLASBuffer = device.CreateBuffer(
				Buffer::Desc{
					.m_Size = sizeInfo.accelerationStructureSize,
					.m_Usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
					.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
				});

			m_TLASScratchBuffer = device.CreateBuffer(
				Buffer::Desc{
					.m_Size = std::max( sizeInfo.buildScratchSize, sizeInfo.updateScratchSize ),
					.m_Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
				});

			VkAccelerationStructureCreateInfoKHR accelerationInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			accelerationInfo.buffer = device.GetBuffer(m_TLASBuffer);
			accelerationInfo.size = sizeInfo.accelerationStructureSize;
			accelerationInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

			VkResult result = vkCreateAccelerationStructureKHR( device.GetDevice(), &accelerationInfo, nullptr, &m_TLAS );
			assert(result == VK_SUCCESS);
		}
	}

	void Scene::BuildTLAS( Device& device, CommandBuffer& commandBuffer ) {
		Buffer& tlasInstanceBuffer = device.GetBuffer( m_TLASInstances );
		Buffer& tlasScratchBuffer = device.GetBuffer( m_TLASScratchBuffer );

		VkAccelerationStructureGeometryKHR geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		geometry.geometry.instances.data.deviceAddress = tlasInstanceBuffer.GetDeviceAddress();

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;
		buildInfo.srcAccelerationStructure = m_TLAS;
		buildInfo.dstAccelerationStructure = m_TLAS;
		buildInfo.scratchData.deviceAddress = tlasScratchBuffer.GetDeviceAddress();

		uint32_t tlasInstancesCount = uint32_t( tlasInstanceBuffer.GetSize() ) / sizeof( VkAccelerationStructureInstanceKHR );

		VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
		buildRange.primitiveCount = tlasInstancesCount;
		
		VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;

		vkCmdBuildAccelerationStructuresKHR( commandBuffer, 1, &buildInfo, &buildRangePtr );

		VkAccessFlags accessFlags = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		VkUtil::CommandBufferStageBarrier( commandBuffer,  VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, accessFlags, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, accessFlags );

		VkWriteDescriptorSetAccelerationStructureKHR asInfo = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &m_TLAS;

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		write.pNext = &asInfo;
		write.dstBinding = 1;
		write.dstSet = device.GetGlobalResources();
		write.descriptorCount = 1;
		write.dstArrayElement = 0;

		vkUpdateDescriptorSets( device.GetDevice(), 1, &write, 0, nullptr );
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
			gpuMat.m_Index = mat.m_Index;
			gpuMat.m_MetallicFactor = mat.m_MetallicFactor;
			gpuMat.m_RoughnessFactor = mat.m_RoughnessFactor;
			gpuMat.m_AlphaMode = mat.m_AlphaMode;
			gpuMat.m_AlphaCutoff = mat.m_AlphaCutoff;
			gpuMat.m_AlbedoTexture = mat.m_AlbedoTexture;
			gpuMat.m_NormalsTexture = mat.m_NormalsTexture;
			gpuMat.m_MetalRoughnessTexture = mat.m_MetalRoughnessTexture;
			gpuMat.m_EmissiveTexture = mat.m_EmissiveTexture;
			gpuMat.m_Albedo = mat.m_Albedo;
			gpuMat.m_Emissive = mat.m_Emissive;

			materials.push_back( gpuMat );
		}

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
				.m_Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
			}
		);

		stagingBuffer->Patch( materials.data(), materials.size() * sizeof( GPUMaterial ) );

		{
			CommandBuffer commandBuffer = CommandBuffer( device );
			VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
			VkUtil::CommandBufferCopyBuffer( commandBuffer, *stagingBuffer, device.GetBuffer( m_MaterialBuffer ), bufferSize );
			VkUtil::CommandBufferEnd( commandBuffer );
			VkUtil::CommandBufferSubmit( commandBuffer, device.GetGraphicsQueue() );
			// vkFreeCommandBuffers( device.GetDevice(), device.GetCommandPool(), 1, &commandBuffer );
		}
	}

	void Scene::UpdateTransformsRecursive( entt::entity entity ) {
		const auto& relation = m_Registry.get<EntityRelation>( entity );
		const auto  parent = GetParent( entity );

		auto& transform = m_Registry.get<Transform>( entity );
		if ( parent == entt::null || parent == m_RootEntity ) {
			transform.m_WorldTransform = transform.m_LocalTransform;
		} else {
			const Transform& parentTransform = m_Registry.get<Transform>( parent );
			transform.m_WorldTransform = parentTransform.m_WorldTransform * transform.m_LocalTransform;
		}

		for ( entt::entity child : relation.m_Children )
			UpdateTransformsRecursive( child );
	}

	void Scene::UpdateTransforms() { 
		UpdateTransformsRecursive(m_RootEntity);
	}

	entt::entity Scene::GetParent( entt::entity entity ) {
		return m_Registry.get<EntityRelation>( entity ).m_Parent;
	}

	void Scene::ParentTo( entt::entity entity, entt::entity parent ) { 
		EntityRelation& rel = m_Registry.get<EntityRelation>(entity);
		if(rel.m_Parent == parent)
			return;

		rel.m_Parent = parent;

		EntityRelation& parentRel = m_Registry.get<EntityRelation>(parent);
		parentRel.m_Children.push_back(entity);
	}

	entt::entity Scene::CreateGameObject() {
		entt::entity ent = m_Registry.create();

		m_Registry.emplace<Transform>(ent);
		m_Registry.emplace<EntityRelation>(ent);

		ParentTo( ent, m_RootEntity );
		return ent;
	}
}