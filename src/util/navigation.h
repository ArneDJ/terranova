#pragma once
#include "../extern/recast/Recast.h"
#include "../extern/recast/DetourNavMesh.h"
#include "../extern/recast/DetourNavMeshBuilder.h"
#include "../extern/recast/DetourNavMeshQuery.h"
#include "../extern/recast/ChunkyTriMesh.h"

namespace util {

enum PolyAreaType {
	POLY_AREA_GROUND = 1,
	POLY_AREA_WATER,
	POLY_AREA_ROAD,
	POLY_AREA_DOOR,
	POLY_AREA_GRASS,
	POLY_AREA_JUMP,
};

enum PolyFlags {
	POLY_FLAGS_WALK = 0x01,  // Ability to walk (ground, grass, road)
	POLY_FLAGS_SWIM = 0x02,  // Ability to swim (water).
	POLY_FLAGS_DOOR = 0x04,  // Ability to move through doors.
	POLY_FLAGS_JUMP = 0x08,  // Ability to jump.
	POLY_FLAGS_DISABLED = 0x10,  // Disabled polygon
	POLY_FLAGS_ALL = 0xffff // All abilities.
};

struct PolySearchResult {
	bool found = false;
	glm::vec3 position = {};
	dtPolyRef poly;
};

struct NavigationTileRecord {
	int x = 0;
	int y = 0;
	std::vector<uint8_t> data;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(x, y, data);
	}
};

struct NavigationMeshRecord {
	glm::vec3 origin = {};
	float tile_width = 0.f;
	float tile_height = 0.f;
	int max_tiles = 0;
	int max_polys = 0;
	std::vector<NavigationTileRecord> tiles;

	void insert(const dtNavMesh *navmesh)
	{
		const dtNavMeshParams *params = navmesh->getParams();
		origin = glm::vec3(params->orig[0], params->orig[1], params->orig[2]);
		tile_width = params->tileWidth;
		tile_height = params->tileHeight;
		max_tiles = params->maxTiles;
		max_polys = params->maxPolys;

		tiles.clear();
		for (int i = 0; i < navmesh->getMaxTiles(); i++) {
			const dtMeshTile *tile = navmesh->getTile(i);
			if (!tile) { continue; }
			if (!tile->header) { continue; }
			const dtMeshHeader *header = tile->header;
			NavigationTileRecord record;
			record.x = header->x;
			record.y = header->y;
			record.data.insert(record.data.end(), tile->data, tile->data + tile->dataSize);
			tiles.push_back(record);
		}
	}

	template <class Archive>
	void save(Archive &archive) const
	{
		archive(origin, tile_width, tile_height, max_tiles, max_polys, tiles);
	}
	template <class Archive>
	void load(Archive &archive, dtNavMesh *navmesh)
	{
		archive(origin, tile_width, tile_height, max_tiles, max_polys, tiles);

		dtNavMeshParams params;
		rcVcopy(params.orig, glm::value_ptr(origin));
		params.tileWidth = tile_width;
		params.tileHeight = tile_height;
		params.maxTiles = max_tiles;
		params.maxPolys = max_polys;

		dtStatus status = navmesh->init(&params);

		for (const auto &tile : tiles) {
			// Remove any previous data (navmesh owns and deletes the data).
			navmesh->removeTile(navmesh->getTileRefAt(tile.x, tile.y, 0), 0, 0);
			// make a copy of the data so the tilemesh can owns it
			uint8_t *cpy = new uint8_t[tile.data.size()];
			std::copy(tile.data.begin(), tile.data.end(), cpy);
			// Let the navmesh own the data.
			dtStatus status = navmesh->addTile(cpy, tile.data.size(), DT_TILE_FREE_DATA, 0, 0);
			if (dtStatusFailed(status)) { dtFree(cpy); }
		}
	}
};

class Navigation {
public:
	Navigation();
public:
	const dtNavMesh* navmesh() const { return m_navmesh.get(); }	
	dtNavMesh* navmesh() { return m_navmesh.get(); }	
 	const dtNavMeshQuery* query() const { return m_query.get(); }
public:
	bool build(const std::vector<float> &vertices, const std::vector<int> &indices);
public:	
	void find_2D_path(const glm::vec2 &startpos, const glm::vec2 &endpos, std::list<glm::vec2> &pathways) const;
	void find_3D_path(const glm::vec3 &startpos, const glm::vec3 &endpos, std::vector<glm::vec3> &pathways) const;
	PolySearchResult point_on_navmesh(const glm::vec3 &point) const;
public:
	template <class Archive>
	void save(Archive &archive) const
	{
		m_record.save(archive);
	}
	template <class Archive>
	void load(Archive &archive)
	{
		m_record.load(archive, m_navmesh.get());

		dtStatus status = m_query->init(m_navmesh.get(), 2048);
	}
private:
	std::unique_ptr<dtNavMesh> m_navmesh;
	std::unique_ptr<dtNavMeshQuery> m_query;
private:
	rcConfig m_config;	
	int m_max_tiles = 0;
	int m_max_polys = 0;
	glm::vec3 m_bounds_min = {};
	glm::vec3 m_bounds_max = {};
	std::unique_ptr<rcChunkyTriMesh> m_chunky_mesh;
	NavigationMeshRecord m_record;
private:
	void build_all_tiles(const std::vector<float> &vertices, const std::vector<int> &indices);
};

};
