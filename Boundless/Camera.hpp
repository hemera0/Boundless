#pragma once

namespace Boundless {
	class Camera {
	public:
		// Redo.
		static Camera StationaryLookAtCamera(
			const glm::vec3& cameraPos,
			const glm::vec3& targetPos,
			const glm::vec3& worldUp,
			const float fov,
			const float aspect,
			const float nearZ );

		// TODO: Redo.
		glm::mat4 GetViewMatrix() const { return glm::lookAt(m_Position, m_Position + m_Front, m_Up); }
		glm::mat4 GetInvViewMatrix() const { return glm::inverse( GetViewMatrix() ); }
		glm::mat4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

		void Update(float dt);
	private:
		void UpdateMouseControls( float xpos, float ypos );
		void UpdateCameraVectors();
		void UpdateMovementControls( float dt, float forwardMove, float sideMove, float upMove );

		glm::mat4 m_ViewMatrix;
		glm::mat4 m_InvViewMatrix;
		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewProjectionMatrix;
		glm::mat4 m_InvViewProjectionMatrix;
		glm::vec3 m_Front;
		glm::vec3 m_Right;
		glm::vec3 m_Up = { 0.f, 1.f, 0.f };
		glm::vec3 m_Position;
		float	  m_Speed = 25.f;
		float	  m_Yaw = 0.f;
		float	  m_Pitch = 0.f;
	};
}