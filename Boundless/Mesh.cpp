#include "Pch.hpp"
#include "Mesh.hpp"

namespace Boundless {
	void Mesh::PackVertexData() {
		if(!m_Vertices.empty())
			return;

		bool hasUVs = !m_Texcoords.empty();
		bool hasNormals = !m_Normals.empty();

		for(auto i = 0; i < m_Positions.size(); i++) {
			MeshVertexData mvd = {};	
			mvd.m_Position = m_Positions[i];
		
			if(hasUVs) {
				mvd.m_UVx = m_Texcoords[i].x;
				mvd.m_UVy = m_Texcoords[i].y;
			}

			if(hasNormals) {
				mvd.m_Normal = m_Normals[i];
			}

			m_Vertices.push_back(mvd);
		}
	}
}