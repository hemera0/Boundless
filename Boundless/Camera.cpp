#include "Pch.hpp"
#include "Camera.hpp"
#include "Engine.hpp"

namespace Boundless {
	// TODO: Interactive camera.
	Camera Camera::StationaryLookAtCamera( const glm::vec3& cameraPos, const glm::vec3& targetPos, const glm::vec3& worldUp, const float fov, const float aspect, const float nearZ ) {
		Camera result = {};
		result.m_ViewMatrix = glm::lookAt( cameraPos, targetPos, worldUp );
		result.m_InvViewMatrix = glm::inverse( result.m_ViewMatrix );
		result.m_ProjectionMatrix = PerspectiveProjection(fov, aspect, nearZ);
		result.m_ViewProjectionMatrix = result.m_ProjectionMatrix * result.m_ViewMatrix;
		return result;
	}
	void Camera::UpdateMouseControls( float xpos, float ypos ) { 
		float x = static_cast< float >( xpos );
		float y = static_cast< float >( ypos );

		static float oldX = x;
		static float oldY = y;

		float dx = oldX - x;
		float dy = oldY - y;

		{
			const float Sensitivity = 0.01f;

			m_Yaw += dx * Sensitivity;
			m_Pitch += dy * Sensitivity;

			m_Pitch = glm::clamp( m_Pitch, glm::radians( -89.9f ), glm::radians( 89.9f ) );

			UpdateCameraVectors();

			oldX = x;
			oldY = y;
		}
	}
	
	void Camera::UpdateCameraVectors() { 
		glm::vec3 front;
		front.x = cos( glm::radians( m_Yaw ) ) * cos( glm::radians( m_Pitch ) );
		front.y = sin( glm::radians( m_Pitch ) );
		front.z = sin( glm::radians( m_Yaw ) ) * cos( glm::radians( m_Pitch ) );

		m_Front = glm::normalize( front );

		m_Right = glm::normalize( glm::cross( front, glm::vec3{ 0.f, 1.f, 0.f } ) );
		m_Up = glm::normalize( glm::cross( m_Right, front ) );
	}
	
	void Camera::UpdateMovementControls( float dt, float forwardMove, float sideMove, float upMove ) {
		float velocity = m_Speed * dt;
		m_Position += ( ( forwardMove * m_Front ) + ( sideMove * m_Right ) + ( upMove * m_Up ) ) * velocity;
	}

	void Camera::Update() {
		// TODO: replace with proper inputs...
		float forwardMove{}, sideMove{}, upMove{};
		if ( GetAsyncKeyState( 'W' ) & 0x8000 )
			forwardMove = 1.f;
		if ( GetAsyncKeyState( 'S' ) & 0x8000 )
			forwardMove = -1.f;
		if ( GetAsyncKeyState( 'A' ) & 0x8000 )
			sideMove = 1.f;
		if ( GetAsyncKeyState( 'D' ) & 0x8000 )
			sideMove = -1.f;
		if( GetAsyncKeyState(VK_SPACE) & 0x8000 )
			upMove = 1.f;
		if ( GetAsyncKeyState( VK_LCONTROL ) & 0x8000 )
			upMove = -1.f;

		// const auto& mousePos = InputSystem::GetMousePos();
		// UpdateMouseControls( mousePos.x, mousePos.y );
		
		UpdateCameraVectors();
		UpdateMovementControls( 0.01f, forwardMove, sideMove, upMove );

		m_ViewMatrix = GetViewMatrix();
		m_InvViewMatrix = glm::inverse( m_ViewMatrix );
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
}