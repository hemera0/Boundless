#pragma once
#include "Scene.hpp"

#include <tinygltf/tiny_gltf.h>

namespace Boundless {
	class GLTFImporter {
	public:
		GLTFImporter( Scene& inScene );

		bool LoadFromFile( const std::string& path );
	private:
		void LoadMesh(entt::entity entity, const tinygltf::Model& model, const tinygltf::Primitive& primitive);
		void LoadMaterials( entt::entity entity, const tinygltf::Model& model );
		void LoadAnimations( entt::entity entity, const tinygltf::Model& model );
		void LoadNode( entt::entity parent, const tinygltf::Model& model, const tinygltf::Node& node, glm::mat4 parentTransform );

		Scene& m_Scene;
		std::string m_BaseDir{};

		std::vector<entt::entity> m_Animations{};
		std::vector<entt::entity> m_Materials{};
	};
}