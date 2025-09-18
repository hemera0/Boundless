#include "Camera.hpp"

namespace Boundless {
	// TODO: Interactive camera.
	Camera Camera::StationaryLookAtCamera( const glm::vec3& cameraPos, const glm::vec3& targetPos, const glm::vec3& worldUp, const float fov, const float aspect, const float nearZ, const float farZ ) {
		Camera result = {};
		result.m_ViewMatrix = glm::lookAt( cameraPos, targetPos, worldUp );
		result.m_InvViewMatrix = glm::inverse( result.m_ViewMatrix );
		result.m_ProjectionMatrix = glm::perspectiveLH( glm::radians( fov ), aspect, farZ, nearZ );
		result.m_ViewProjectionMatrix = result.m_ProjectionMatrix * result.m_ViewMatrix;
		return result;
	}
}