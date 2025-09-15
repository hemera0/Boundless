#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Boundless {
	class Camera {
	public:
		static Camera StationaryLookAtCamera(
			const glm::vec3& cameraPos,
			const glm::vec3& targetPos,
			const glm::vec3& worldUp,
			const float fov,
			const float aspect,
			const float nearZ,
			const float farZ );

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetInvViewMatrix() const { return m_InvViewMatrix; }
		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
	private:
		glm::mat4 m_ViewMatrix{};
		glm::mat4 m_InvViewMatrix{};
		glm::mat4 m_ProjectionMatrix{};
		glm::mat4 m_ViewProjectionMatrix{};
	};
}