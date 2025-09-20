#pragma once
#include "Device.hpp"

#include <entt/entt.hpp>

#include "Mesh.hpp"
#include "Camera.hpp"

namespace Boundless {
	struct SceneInfo {
		glm::mat4 m_CameraViewProjectionMatrix{};
		glm::vec4 m_SunDirection{};
		glm::vec4 m_SunColor{};
	};

	class Scene {
	public:
		entt::registry& GetRegistry() { return m_Registry; }

		Camera& GetMainCamera() { return m_MainCamera; }
		void SetMainCamera( const Camera& camera ) { m_MainCamera = camera; }

		void OnDeviceStart( const std::unique_ptr<Device>& device );

		void UploadMeshes( const std::unique_ptr<Device>& device );
		void UploadTextures( const std::unique_ptr<Device>& device );
		void UploadMaterials( const std::unique_ptr<Device>& device );

		BufferHandle GetUniformBuffer() const { return m_UniformBuffer; }
		BufferHandle GetMaterialBuffer() const { return m_MaterialBuffer; }
	private:
		Camera m_MainCamera{};
		entt::registry m_Registry{};
	
		BufferHandle m_UniformBuffer{};
		BufferHandle m_MaterialBuffer{};
	};
}