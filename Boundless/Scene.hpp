#pragma once
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

		void OnTick();

		void Render();

		const Camera& GetMainCamera() const { return m_MainCamera; }
		void SetMainCamera( const Camera& camera ) { m_MainCamera = camera; }
	private:
		Camera m_MainCamera{};
		entt::registry m_Registry{};
	};
}