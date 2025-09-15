#pragma once
#include "Scene.hpp"

namespace Boundless {
	class OBJImporter {
	public:
		OBJImporter(Scene& inScene);

		bool LoadFromFile(const std::string& path);
	private:
		Scene& m_Scene;
	};
}