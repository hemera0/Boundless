#include "OBJImporter.hpp"
#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

Boundless::OBJImporter::OBJImporter( Scene& inScene ) : m_Scene( inScene ) { }

bool Boundless::OBJImporter::LoadFromFile( const std::string& path ) {
	auto& registry = m_Scene.GetRegistry();

	tinyobj::attrib_t inattrib;
	std::vector<tinyobj::shape_t> inshapes;
	std::vector<tinyobj::material_t> inmaterials;
	std::string err;

	auto fileDir = std::filesystem::path( path ).remove_filename().string();

	bool ret = tinyobj::LoadObj( &inattrib, &inshapes, &inmaterials, &err, path.c_str(), fileDir.c_str() );
	if ( !err.empty() ) {
		printf( "ERR: %s\n", err.c_str() );
	}

	if ( !ret )
		return false;

	for ( size_t i = 0; i < inshapes.size(); i++ ) {
		const auto& inMesh = inshapes[ i ].mesh;

		entt::entity meshEntity = registry.create();

		Mesh& meshComponent = registry.emplace<Mesh>( meshEntity );
		for ( const auto& index : inMesh.indices ) {
			int vertex_index = index.vertex_index * 3;
			int normal_index = index.normal_index * 3;
			int texcoord_index = index.texcoord_index * 2;

			MeshVertexData vd = {};
			vd.m_Position = { inattrib.vertices[ vertex_index + 0 ],  inattrib.vertices[ vertex_index + 1 ],  inattrib.vertices[ vertex_index + 2 ] };
			vd.m_Normal = { inattrib.normals[ normal_index + 0 ], inattrib.normals[ normal_index + 1 ], inattrib.normals[ normal_index + 2 ] };
			vd.m_UVx = inattrib.vertices[ texcoord_index + 0 ];
			vd.m_UVy = inattrib.vertices[ texcoord_index + 1 ];

			meshComponent.m_Name = std::string( inshapes[ i ].name );
			meshComponent.m_Vertices.push_back( vd );
		}
	}

	return true;
}