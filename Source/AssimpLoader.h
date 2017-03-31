#pragma once
#include <stddef.h>

class AssimpLoader
{
public:

	struct Vector3D
	{
		float x, y, z;
	};

	struct Vector2D
	{
		float x, y;
	};

	struct Vertex
	{
		Vector3D  position;
		Vector3D  normal;
		Vector3D  tangent;
		Vector2D  uv;
	};

	static constexpr size_t VertexSize = sizeof(Vertex);
	static constexpr unsigned int PositionOffset = offsetof(Vertex, position);
	static constexpr unsigned int NormalOffset = offsetof(Vertex, normal);
	static constexpr unsigned int TangentOffset = offsetof(Vertex, tangent);
	static constexpr unsigned int UVOffset = offsetof(Vertex, uv);

public:
	AssimpLoader() 
		:
		vertices(nullptr), 
		normals(nullptr), 
		tangents(nullptr), 
		uvs(nullptr), 
		indices(nullptr),
		numVertices(0),
		numIndices(0)
	{}

	~AssimpLoader();

	void Release();

	void LoadFromFile(const char* filename);

	void FillInVerticesData(void* pDest) const;
	size_t GetVerticesCount() const { return numVertices; }

	const unsigned int* GetIndices() const { return indices; }
	size_t GetIndicesCount() const { return numIndices; }

private:
	Vector3D*		vertices;
	Vector3D*		normals;
	Vector3D*		tangents;
	Vector2D*		uvs;
	unsigned int*	indices;
	size_t			numVertices;
	size_t			numIndices;
};
