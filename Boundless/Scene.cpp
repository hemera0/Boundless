#include "Scene.hpp"
#include "Engine.hpp"

namespace Boundless {
	void Scene::OnDeviceStart( const std::unique_ptr<Device>& device ) { 
		m_UniformBuffer = device->CreateBuffer(
			Buffer::Desc{
				.m_Size = sizeof( SceneInfo ),
				.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO,
				.m_Mappable = true
			}
		);

		UploadMeshes( device );
		UploadTextures( device );
		UploadMaterials( device );
	}
	
	void Scene::UploadMeshes( const std::unique_ptr<Device>& device ) { 
		for ( entt::entity meshEntities : m_Registry.view<Mesh>() ) {
			Mesh& mesh = m_Registry.get<Mesh>( meshEntities );

			if ( mesh.m_VertexBuffer == BufferHandle::Invalid ) {
				mesh.m_VertexBuffer = device->CreateBuffer(
					Buffer::Desc{
						.m_Size = mesh.m_Vertices.size() * sizeof( MeshVertexData ),
						.m_Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);
			}

			if ( mesh.m_IndexBuffer == BufferHandle::Invalid && !mesh.m_Indices.empty() ) {
				mesh.m_IndexBuffer = device->CreateBuffer(
					Buffer::Desc{
						.m_Size = mesh.m_Indices.size() * sizeof( uint32_t ),
						.m_Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
						.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
					}
				);
			}

			if ( !mesh.m_Indices.empty() ) {
				Buffer* indexBuffer = device->GetBuffer( mesh.m_IndexBuffer );

				size_t bufferSize = mesh.m_Indices.size() * sizeof( uint32_t );
				std::unique_ptr<StagingBuffer> stagingBuffer = device->CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_Indices.data(), bufferSize );

				VkCommandBuffer commandBuffer = device->CreateCommandBuffer();

				VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
				VkUtil::CommandBufferCopyBuffer( commandBuffer, *stagingBuffer, *indexBuffer, bufferSize );
				VkUtil::CommandBufferEnd( commandBuffer );
				VkUtil::CommandBufferSubmit( commandBuffer, device->GetGraphicsQueue() );

				vkFreeCommandBuffers( device->GetDevice(), device->GetCommandPool(), 1, &commandBuffer );
			}

			{
				Buffer* vertexBuffer = device->GetBuffer( mesh.m_VertexBuffer );

				size_t bufferSize = mesh.m_Vertices.size() * sizeof( MeshVertexData );
				std::unique_ptr<StagingBuffer> stagingBuffer = device->CreateStagingBuffer( bufferSize );
				stagingBuffer->Patch( mesh.m_Vertices.data(), bufferSize );

				VkCommandBuffer commandBuffer = device->CreateCommandBuffer();

				VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
				VkUtil::CommandBufferCopyBuffer( commandBuffer, *stagingBuffer, *vertexBuffer, bufferSize );
				VkUtil::CommandBufferEnd( commandBuffer );
				VkUtil::CommandBufferSubmit( commandBuffer, device->GetGraphicsQueue() );

				vkFreeCommandBuffers( device->GetDevice(), device->GetCommandPool(), 1, &commandBuffer );
			}
		}
	}
	
	void Scene::UploadTextures( const std::unique_ptr<Device>& device ) { 
		for ( auto ent : m_Registry.view<Material>() ) {
			Material& mat = m_Registry.get<Material>( ent );

			if ( !mat.m_AlbedoTexturePath.empty() ) {
				Image image = device->LoadImageFromFile( mat.m_AlbedoTexturePath, true );
				mat.m_AlbedoTexture = device->CreateTexture( image.GetView(), device->CreateSampler( mat.m_AlbedoSampler ) );
			}

			if ( !mat.m_NormalsTexturePath.empty() ) {
				Image image = device->LoadImageFromFile( mat.m_NormalsTexturePath, false );
				mat.m_NormalsTexture = device->CreateTexture( image.GetView(), device->CreateSampler( mat.m_AlbedoSampler ) );
			}

			if ( !mat.m_EmissiveTexturePath.empty() ) {
				Image image = device->LoadImageFromFile( mat.m_NormalsTexturePath, false );
				mat.m_EmissiveTexture = device->CreateTexture( image.GetView(), device->CreateSampler( mat.m_AlbedoSampler ) );
			}
		}
	}
	
	void Scene::UploadMaterials( const std::unique_ptr<Device>& device ) { 
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

		std::sort( materials.begin(), materials.end(), [ & ]( const auto& a, const auto& b ) -> bool
		{
			return a.m_Index < b.m_Index;
		} );

		size_t bufferSize = materials.size() * sizeof( GPUMaterial );

		std::unique_ptr<StagingBuffer> stagingBuffer = device->CreateStagingBuffer( bufferSize );

		m_MaterialBuffer = device->CreateBuffer(
			Buffer::Desc{
				.m_Size = bufferSize,
				.m_Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.m_MemoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
			}
		);

		stagingBuffer->Patch( materials.data(), materials.size() * sizeof( GPUMaterial ) );

		VkCommandBuffer commandBuffer = device->CreateCommandBuffer();

		VkUtil::CommandBufferBegin( commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
		VkUtil::CommandBufferCopyBuffer( commandBuffer, *stagingBuffer, *device->GetBuffer( m_MaterialBuffer ), bufferSize );
		VkUtil::CommandBufferEnd( commandBuffer );
		VkUtil::CommandBufferSubmit( commandBuffer, device->GetGraphicsQueue() );

		vkFreeCommandBuffers( device->GetDevice(), device->GetCommandPool(), 1, &commandBuffer );
	}
}