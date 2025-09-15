#include "Camera.hpp"

Boundless::Camera Boundless::Camera::StationaryLookAtCamera( const glm::vec3& cameraPos, const glm::vec3& targetPos, const glm::vec3& worldUp, const float fov, const float aspect, const float nearZ, const float farZ ) {
	Camera result = {};
	result.m_ViewMatrix = glm::lookAt( cameraPos, targetPos, worldUp );
	result.m_InvViewMatrix = glm::inverse( result.m_ViewMatrix );
	result.m_ProjectionMatrix = glm::perspective( fov, aspect, nearZ, farZ );
	result.m_ProjectionMatrix[ 1 ][ 1 ] *= -1;
	result.m_ViewProjectionMatrix = result.m_ProjectionMatrix * result.m_ViewMatrix;
	return result;
}