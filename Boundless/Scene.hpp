#pragma once
#include <entt/entt.hpp>
#include "Mesh.hpp"
#include "Camera.hpp"

namespace Boundless {
	class Scene {
	public:
		entt::registry& GetRegistry() { return m_Registry; }
	
		void OnTick();

		void Render();

		const Camera& GetMainCamera() const { return m_MainCamera; }
		void SetMainCamera(const Camera& camera) { m_MainCamera = camera; }
	private:
		Camera m_MainCamera{};
		entt::registry m_Registry{};
	};
}