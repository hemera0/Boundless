#pragma once
#include "Pch.hpp"
#include "Device.hpp"
#include "Components.hpp"

namespace Boundless {
	struct EntityTag {
		std::string m_Name;
	};

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

		void UploadToGPU( Device& device );
		void BuildTLAS( Device& device, CommandBuffer& commandBuffer );
		void UpdateTransforms();
		void UpdateAnimations( float deltaTime );

		BufferHandle GetMaterialBuffer() const { return m_MaterialBuffer; }
		vk::AccelerationStructureKHR GetTLAS() const { return m_TLAS; }

		// Entity relations.
		entt::entity GetParent(entt::entity entity);
		void ParentTo(entt::entity entity, entt::entity parent);
		
		entt::entity CreatedNamedEntity( const std::string& name = "" );
		entt::entity CreateEntityWithTransform( const std::string& name = "" );
		entt::entity GetRootEntity() const { return m_RootEntity; }
	private:
		void UploadTLAS( Device& device );
		void UploadMeshes( Device& device );
		void UploadTextures( Device& device );
		void UploadMaterials( Device& device );
		void UploadSkeletons( Device& device );
		void UpdateTransformsRecursive( entt::entity entity, const glm::mat4& parentTransform );

		Camera						 m_MainCamera; // TODO: Remove.
		entt::registry				 m_Registry;
		entt::entity				 m_RootEntity;
		BufferHandle				 m_MaterialBuffer = BufferHandle::Invalid;

		// Ray-tracing Data.
		vk::AccelerationStructureKHR m_TLAS = {};
		BufferHandle				 m_TLASBuffer = BufferHandle::Invalid;
		BufferHandle				 m_TLASScratchBuffer = BufferHandle::Invalid;
		BufferHandle				 m_TLASInstances = BufferHandle::Invalid;
	};
}