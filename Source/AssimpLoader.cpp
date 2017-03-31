#include "AssimpLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#if defined(_DEBUG)
#pragma comment(lib, "assimpd.lib")
#pragma comment(lib, "zlibstaticd.lib")
#else
#pragma comment(lib, "assimp.lib")
#pragma comment(lib, "zlibstatic.lib")
#endif

AssimpLoader::~AssimpLoader()
{
	Release();
}

void AssimpLoader::Release()
{
	delete[] vertices;
	delete[] normals;
	delete[] tangents;
	delete[] uvs;
	delete[] indices;
	numVertices = 0;
	numIndices = 0;
}

void AssimpLoader::LoadFromFile(const char * filename)
{
	Release();

	Assimp::Importer importer;
	auto scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

	if (!scene)
		return;

	size_t vertsInTotal = 0, facesInTotal = 0;

	for (size_t i = 0; i < scene->mNumMeshes; ++i)
	{
		auto mesh = scene->mMeshes[i];

		if (!mesh->HasNormals() || !mesh->HasTextureCoords(0) || !mesh->HasTangentsAndBitangents())
			continue;

		vertsInTotal += mesh->mNumVertices;
		facesInTotal += mesh->mNumFaces;
	}

	vertices = new Vector3D[vertsInTotal];
	normals = new Vector3D[vertsInTotal];
	tangents = new Vector3D[vertsInTotal];
	uvs = new Vector2D[vertsInTotal];
	indices = new unsigned int[facesInTotal * 3];

	unsigned int start_vertex_index = 0;
	unsigned int  start_face_index = 0;

	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		auto mesh = scene->mMeshes[i];

		if (!mesh->HasNormals() || !mesh->HasTextureCoords(0) || !mesh->HasTangentsAndBitangents())
			continue;

		for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
		{
			aiVector3D& pos = reinterpret_cast<aiVector3D&>(vertices[start_vertex_index + j]);
			aiVector3D& norm = reinterpret_cast<aiVector3D&>(normals[start_vertex_index + j]);
			aiVector3D& tan = reinterpret_cast<aiVector3D&>(tangents[start_vertex_index + j]);
			aiVector2D& uv = reinterpret_cast<aiVector2D&>(uvs[start_vertex_index + j]);

			pos = mesh->mVertices[j];
			norm = mesh->mNormals[j];
			tan = mesh->mTangents[j];
			auto uv3 = mesh->mTextureCoords[0][j];
			uv.x = uv3.x;
			uv.y = uv3.y;
		}

		for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
		{
			auto face = mesh->mFaces[j];
			unsigned int* pIndices = &indices[(start_face_index + j) * 3];
			pIndices[0] = face.mIndices[0] + start_vertex_index;
			pIndices[1] = face.mIndices[1] + start_vertex_index;
			pIndices[2] = face.mIndices[2] + start_vertex_index;
		}

		start_vertex_index += mesh->mNumVertices;
		start_face_index += mesh->mNumFaces;
	}

	numVertices = vertsInTotal;
	numIndices = facesInTotal * 3;
}

void AssimpLoader::FillInVerticesData(void * pDest) const
{
	Vertex* arr = reinterpret_cast<Vertex*>(pDest);
	for (size_t i = 0; i < numVertices; i++)
	{
		arr[i].position = vertices[i];
		arr[i].normal = normals[i];
		arr[i].tangent = tangents[i];
		arr[i].uv = uvs[i];
	}
}
