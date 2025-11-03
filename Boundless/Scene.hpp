#pragma once
#include "Device.hpp"

#include <entt/entt.hpp>

#include "Mesh.hpp"
#include "Camera.hpp"

namespace Boundless {

	// TODO: Move this back to Engine.hpp and call it FrameInfo/FrameConstants
	struct SceneInfo {
		glm::mat4 m_CameraViewProjectionMatrix{};
		glm::mat4 m_CameraInvViewProjectionMatrix{};
		glm::vec4 m_CameraPosition{};
		glm::vec4 m_SunDirection{};
		glm::vec4 m_SunColor{};
		glm::ivec4 m_IblTextures{};
	};

	struct EntityRelation { 
		entt::entity m_Parent = entt::null;
		std::vector<entt::entity> m_Children{};
	};

	class Scene {
	public:
		Scene();

		entt::registry& GetRegistry() { return m_Registry; }

		Camera& GetMainCamera() { return m_MainCamera; }
		void SetMainCamera( const Camera& camera ) { m_MainCamera = camera; }

		void OnDeviceStart( const std::unique_ptr<Device>& device );

		void UploadMeshes( const std::unique_ptr<Device>& device );
		void UploadTLAS( const std::unique_ptr<Device>& device);
		void BuildTLAS( const std::unique_ptr<Device>& device, VkCommandBuffer commandBuffer );

		void UploadTextures( const std::unique_ptr<Device>& device );
		void UploadMaterials( const std::unique_ptr<Device>& device );

		void UpdateTransformsRecursive( entt::entity entity );
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
		Camera m_MainCamera{};
		
		entt::registry m_Registry{};
		entt::entity m_RootEntity{};

		// Rendering...
		BufferHandle m_UniformBuffer  = BufferHandle::Invalid;
		BufferHandle m_MaterialBuffer = BufferHandle::Invalid;

		// RT...
		// todo: EWWWWWWWWWW...
		uint32_t m_TotalPrimitiveCount{};

		VkAccelerationStructureKHR m_TLAS{};
		BufferHandle m_TLASBuffer		 = BufferHandle::Invalid;
		BufferHandle m_TLASScratchBuffer = BufferHandle::Invalid;
		BufferHandle m_TLASInstances	 = BufferHandle::Invalid;
	};
}