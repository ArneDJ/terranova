#pragma once
#include "../extern/recast/Recast.h"
#include "../extern/recast/DetourNavMesh.h"
#include "../extern/recast/DetourNavMeshBuilder.h"
#include "../extern/recast/DetourNavMeshQuery.h"
#include "../extern/recast/ChunkyTriMesh.h"

namespace util {

enum sample_poly_areas {
	SAMPLE_POLYAREA_GROUND,
	SAMPLE_POLYAREA_WATER,
	SAMPLE_POLYAREA_ROAD,
	SAMPLE_POLYAREA_DOOR,
	SAMPLE_POLYAREA_GRASS,
	SAMPLE_POLYAREA_JUMP,
};

enum sample_poly_flags {
	SAMPLE_POLYFLAGS_WALK = 0x01,  // Ability to walk (ground, grass, road)
	SAMPLE_POLYFLAGS_SWIM = 0x02,  // Ability to swim (water).
	SAMPLE_POLYFLAGS_DOOR = 0x04,  // Ability to move through doors.
	SAMPLE_POLYFLAGS_JUMP = 0x08,  // Ability to jump.
	SAMPLE_POLYFLAGS_DISABLED = 0x10,  // Disabled polygon
	SAMPLE_POLYFLAGS_ALL = 0xffff // All abilities.
};

struct poly_result_t {
	bool found = false;
	glm::vec3 position = {};
	dtPolyRef poly;
};

class Navigation
{
public:
	Navigation();
public:
	const dtNavMesh* get_navmesh() const { return navmesh.get(); }	
	dtNavMesh* get_navmesh() { return navmesh.get(); }	
 	const dtNavMeshQuery* get_navquery() const { return navquery.get(); }
public:
	bool alloc(const glm::vec3 &origin, float tilewidth, float tileheight, int maxtiles, int maxpolys);
	void cleanup();
	bool build(const std::vector<float> &vertices, const std::vector<int> &indices);
	void load_tilemesh(int x, int y, const std::vector<uint8_t> &data);
public:	
	void find_2D_path(const glm::vec2 &startpos, const glm::vec2 &endpos, std::list<glm::vec2> &pathways) const;
	void find_3D_path(const glm::vec3 &startpos, const glm::vec3 &endpos, std::vector<glm::vec3> &pathways) const;
	poly_result_t point_on_navmesh(const glm::vec3 &point) const;
private:
	std::unique_ptr<dtNavMesh> navmesh;
	std::unique_ptr<dtNavMeshQuery> navquery;
private:
	rcConfig cfg;	
	int max_tiles;
	int max_polys_per_tile;
	int tile_tri_count;
	float BOUNDS_MIN[3];
	float BOUNDS_MAX[3];
	std::vector<float> verts;
	std::vector<int> tris;
	std::unique_ptr<rcChunkyTriMesh> chunky_mesh;
private:
	void build_all_tiles();
	void remove_all_tiles();
};

};
