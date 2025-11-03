#pragma once
#include "VkUtil.hpp"
#include <glm/glm.hpp>

namespace Boundless {
	class Transform {
	public:
		glm::mat4 GetWorldTransform() {
			return m_WorldTransform;
		}
		
		// TODO: finish...

		glm::mat4 m_LocalTransform{};
		glm::mat4 m_WorldTransform{};
	private:
		glm::vec3 m_Translation{}, m_Scale{};
		glm::quat m_Rotation{};

		Transform* m_Parent{};
	};
}