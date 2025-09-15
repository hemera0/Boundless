#pragma once
#include "VkUtil.hpp"
#include <glm/glm.hpp>

namespace Boundless {
	class Transform {
	public:
		glm::mat4 GetWorldTransform() {
			return m_WorldTransform;
		}

	private:
		Transform* m_Parent{};

		glm::mat4 m_LocalTransform{};
		glm::mat4 m_WorldTransform{};
	};
}