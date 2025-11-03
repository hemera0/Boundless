#include "Pch.hpp"
#include "Camera.hpp"
#include "Input.hpp"
#include "Engine.hpp"

namespace Boundless {
	Camera Camera::StationaryLookAtCamera( const glm::vec3& cameraPos, const glm::vec3& targetPos, const glm::vec3& worldUp, const float fov, const float aspect, const float nearZ ) {
		Camera result = {};
		result.m_ViewMatrix = glm::lookAt( cameraPos, targetPos, worldUp );
		result.m_InvViewMatrix = glm::inverse( result.m_ViewMatrix );
		result.m_ProjectionMatrix = glm::perspectiveRH(glm::radians(fov), aspect, nearZ, 4096.f);
		result.m_ViewProjectionMatrix = result.m_ProjectionMatrix * result.m_ViewMatrix;
		return result;
	}

	void Camera::UpdateMouseControls( float xpos, float ypos ) { 
		static float oldX = xpos;
		static float oldY = ypos;

		float dx = xpos - oldX;
		float dy = oldY - ypos;

		const float Sensitivity = 0.01f;
		m_Yaw += dx * Sensitivity;
		m_Pitch += dy * Sensitivity;
		m_Pitch = glm::clamp( m_Pitch, -89.9f, 89.9f );

		UpdateCameraVectors();

		oldX = xpos;
		oldY = ypos;
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

	void Camera::Update( float dt ) {
		// TODO: replace with proper inputs...
		float forwardMove{}, sideMove{}, upMove{};
		if ( g_Input->GetKeyHeld( KeyCode::W ) )
			forwardMove = 1.f;
		if ( g_Input->GetKeyHeld( KeyCode::S ) )
			forwardMove = -1.f;
		if ( g_Input->GetKeyHeld( KeyCode::D ) )
			sideMove = 1.f;
		if ( g_Input->GetKeyHeld( KeyCode::A ) )
			sideMove = -1.f;
		if( g_Input->GetKeyHeld( KeyCode::Space ) )
			upMove = 1.f;
		if ( g_Input->GetKeyHeld( KeyCode::LeftControl ) )
			upMove = -1.f;

		const auto& mousePos = g_Input->GetMousePos();
		UpdateMouseControls( mousePos.x, mousePos.y );
		
		UpdateCameraVectors();
		UpdateMovementControls( dt, forwardMove, sideMove, upMove );

		m_ViewMatrix = GetViewMatrix();
		m_InvViewMatrix = glm::inverse( m_ViewMatrix );
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
}