#pragma once

#include "cdc/math/Vector.h"

struct Intro;
struct Signal;
struct Instance;
struct KDNode;
struct StreamUnit;

struct IndexedFace
{
	unsigned __int16 i0;
	unsigned __int16 i1;
	unsigned __int16 i2;

	unsigned __int8 adjacencyFlags;
	unsigned __int8 collisionFlags;
	unsigned __int8 clientFlags;
	unsigned __int8 materialType;
};

struct SignalFace
{
	unsigned __int16 i0;
	unsigned __int16 i1;
	unsigned __int16 i2;

	char adjacencyFlags;
	char collisionFlags;

	unsigned __int16 id;
};

struct MeshVertex
{
	__int16 x;
	__int16 y;
	__int16 z;
};

struct MeshVertex32
{
	float x;
	float y;
	float z;
	float w;
};

struct BBox
{
	cdc::Vector3 bMin;
	cdc::Vector3 bMax;
};

struct Mesh
{
	BBox m_box;
	cdc::Vector3 m_position;

	void* m_vertices;
	IndexedFace* m_faces;
	KDNode* m_root;

	void* m_clientData;

	unsigned __int16 m_vertexType;
	unsigned __int16 m_numNodes;
	unsigned __int16 m_numFaces;
	unsigned __int16 m_numVertices;
	unsigned __int16 m_height;
	unsigned __int16 m_numDegenerateFaces;
	unsigned __int16 m_numNonManifoldEdges;
};

struct Level;

struct TerrainGroup
{
	cdc::Vector3 globalOffset;
	cdc::Vector3 localOffset;

	int flags;
	int ID;
	int uniqueID;
	int splineID;
	Instance* instanceSpline;
	Level* level;
	Mesh* mesh;

	char pad1[116];
};

struct StreamUnitPortal
{
	char tolevelname[30];

	__int16 toSignalID;
	__int16 MSignalID;
	__int16 streamID;

	unsigned __int16* closeVertList;
	float activeDistance;
	StreamUnit* toStreamUnit;

	cdc::Vector3 min;
	cdc::Vector3 max;
	cdc::Vector3 quad[4];
	cdc::Vector3 normal;
};

struct Terrain
{
	__int16 UnitChangeFlags;
	__int16 spad;

	int numIntros;
	Intro* introList;

	int numStreamUnitPortals;
	StreamUnitPortal* streamUnitPortals;

	int numTerrainGroups;
	TerrainGroup* terrainGroups;

	TerrainGroup* signalTerrainGroup;
	Signal* signals;
};

struct Level
{
	Terrain* terrain;

	char pad1[140];

	Signal* SignalListStart;
	__int16* SignalIDList;
	void* splineCameraData;

	void* relocModule; // Pointer to script executable

	char pad2[84];

	void* pCdcPlannerData;
	void* pAreaDBase;
	void* pUnitData;
};