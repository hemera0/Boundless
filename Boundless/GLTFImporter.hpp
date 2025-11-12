#pragma once
#include "Pch.hpp"
#include "Scene.hpp"

namespace Boundless {
	class GLTFImporter {
	public:
		GLTFImporter( Scene& inScene );

		bool LoadFromFile( const std::string& path );
	private:
		void LoadMesh(entt::entity entity, const tinygltf::Model& model, const tinygltf::Node& node, const tinygltf::Primitive& primitive);
		void LoadMaterials( const tinygltf::Model& model );
		void LoadAnimations( const tinygltf::Model& model );
		void LoadNode( entt::entity parent, const tinygltf::Model& model, const tinygltf::Node& node, glm::mat4 parentTransform );

		// Skeletal animation functions.
		void LoadSkin( const tinygltf::Model& model, const tinygltf::Skin& skin );
		int GetJointIndexForNode( const tinygltf::Skin& skin, const int node );
		void CopyJointHierarchy( const tinygltf::Model& model, const tinygltf::Skin& skin, const tinygltf::Node& node, Bone& bone );

		Scene&					  m_Scene;
		std::string				  m_BaseDir;
		std::vector<entt::entity> m_Animations;
		std::vector<entt::entity> m_Materials;
		std::vector<entt::entity> m_Skins;
	};
}