#pragma once
#include "Pch.hpp"
#include "Device.hpp"
#include "Components.hpp"

namespace Boundless {
	struct EntityRelation { 
		entt::entity			  m_Parent = entt::null;
		std::vector<entt::entity> m_Children{};
	};

	class Scene {
	public:
		Scene();

		entt::registry& GetRegistry() { return m_Registry; }

		// TODO: Remove.
		Camera& GetMainCamera() { return m_MainCamera; }
		void SetMainCamera( const Camera& camera ) { m_MainCamera = camera; }

		void OnDeviceStart( Device& device );
		void BuildTLAS( Device& device, CommandBuffer& commandBuffer );
		void UpdateTransforms();

		BufferHandle GetUniformBuffer() const { return m_UniformBuffer; }
		BufferHandle GetMaterialBuffer() const { return m_MaterialBuffer; }
		VkAccelerationStructureKHR GetTLAS() const { return m_TLAS; }

		// Entity relations
		entt::entity GetParent(entt::entity entity);
		void ParentTo(entt::entity entity, entt::entity parent);
		
		entt::entity CreateGameObject( );
		entt::entity GetRootEntity() const { return m_RootEntity; }
	private:
		void UploadTLAS( Device& device );
		void UploadMeshes( Device& device );
		void UploadTextures( Device& device );
		void UploadMaterials( Device& device );
		void UpdateTransformsRecursive( entt::entity entity );

		Camera					   m_MainCamera; // todo: Remove.
		entt::registry			   m_Registry;
		entt::entity			   m_RootEntity;
		BufferHandle			   m_UniformBuffer = BufferHandle::Invalid; // todo: move to engine
		BufferHandle			   m_MaterialBuffer = BufferHandle::Invalid;		
		VkAccelerationStructureKHR m_TLAS = {};
		BufferHandle			   m_TLASBuffer = BufferHandle::Invalid;
		BufferHandle			   m_TLASScratchBuffer = BufferHandle::Invalid;
		BufferHandle			   m_TLASInstances = BufferHandle::Invalid;
	};
}