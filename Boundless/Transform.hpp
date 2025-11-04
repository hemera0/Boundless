#pragma once
#include "Pch.hpp"

namespace Boundless {
	// TODO: finish...
	class Transform {
	public:
		glm::mat4 GetWorldTransform() {
			return m_WorldTransform;
		}
		
		glm::mat4  m_LocalTransform{};
		glm::mat4  m_WorldTransform{};
	private:
		glm::vec3  m_Translation;
		glm::vec3  m_Scale;
		glm::quat  m_Rotation;
		Transform* m_Parent = nullptr;
	};
}