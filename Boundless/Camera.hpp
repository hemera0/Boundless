#pragma once



namespace Boundless {
	inline glm::mat4 PerspectiveProjection(float fovY, float aspect, float nearZ) {
		float f = 1.0f / glm::tan( glm::radians( fovY ) * 0.5f );

		glm::mat4 result( 0.0f );
		result[ 0 ][ 0 ] = f / aspect;
		result[ 1 ][ 1 ] = f;
		result[ 2 ][ 2 ] = 0.0f;
		result[ 2 ][ 3 ] = -1.0f;
		result[ 3 ][ 2 ] = nearZ;
	
		return result;
	}

	class Camera {
	public:
		static Camera StationaryLookAtCamera(
			const glm::vec3& cameraPos,
			const glm::vec3& targetPos,
			const glm::vec3& worldUp,
			const float fov,
			const float aspect,
			const float nearZ );

		glm::mat4 GetViewMatrix() const { return glm::lookAt(m_Position, m_Position + m_Front, m_Up); }
		glm::mat4 GetInvViewMatrix() const { return glm::inverse( GetViewMatrix() ); }
		glm::mat4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

		void Update(float dt);
	private:
		void UpdateMouseControls( float xpos, float ypos );
		void UpdateCameraVectors();
		void UpdateMovementControls( float dt, float forwardMove, float sideMove, float upMove );

		glm::mat4 m_ViewMatrix{};
		glm::mat4 m_InvViewMatrix{};
		glm::mat4 m_ProjectionMatrix{};
		glm::mat4 m_ViewProjectionMatrix{};
		glm::mat4 m_InvViewProjectionMatrix{};

		float m_Speed{ 25.f };
		float m_Yaw{}, m_Pitch{};

		glm::vec3 m_Front{}, m_Right{}, m_Up{ 0.f, 1.f, 0.f };
		glm::vec3 m_Position{};
	};
}