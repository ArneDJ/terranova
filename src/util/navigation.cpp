#include <math.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cstdio>
#include <vector>
#include <list>
#include <cstring>
#include <thread>
#include <mutex>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "logger.h"
#include "serialize.h"

#include "navigation.h"

#define MAX_PATHPOLY 256 // max number of polygons in a path
#define MAX_PATHVERT 512 // most verts in a path

namespace util {

class Navbuilder {
public:
	Navbuilder(bool ignore_agent_radius)
		: m_ignore_agent_radius(ignore_agent_radius)
	{
		context = new rcContext { false };
	}
	~Navbuilder()
	{
		if (triareas) { delete [] triareas; }
		rcFreeHeightField(solid);
		rcFreeCompactHeightfield(chf);
		rcFreeContourSet(cset);
		rcFreePolyMesh(pmesh);
		rcFreePolyMeshDetail(dmesh);
		delete context;
	}
	uint8_t *alloc_navdata(const int tx, const int ty, float *bmin, float *bmax, int &data_size, const float *verts, const int nverts, const rcChunkyTriMesh *chunky_mesh, const rcConfig *config);
private:
	uint8_t *triareas = 0;
	rcContext *context = nullptr;
	rcHeightfield *solid = 0;
	rcCompactHeightfield *chf = 0;
	rcContourSet *cset = 0;
	rcPolyMesh *pmesh = 0;
	rcPolyMeshDetail *dmesh = 0;
	bool m_ignore_agent_radius = true;
};

static const float BOX_EXTENTS[3] = {512.0f, 512.0f, 512.0f}; // size of box around start/end points to look for nav polygons
// http://digestingduck.blogspot.com/2009/08/recast-settings-uncovered.html
static const float CELL_SIZE = 0.35f;
static const float CELL_HEIGHT = 0.2f;
static const float AGENT_HEIGHT = 2.f;
static const float AGENT_RADIUS = 0.6f;
static const float AGENT_MAX_CLIMB = 0.9f;
static const float AGENT_MAX_SLOPE = 60.f;
static const int REGION_MIN_SIZE = 8;
static const int REGION_MERGE_SIZE = 20;
static const float EDGE_MAX_LEN = 12.f;
static const float EDGE_MAX_ERROR = 1.3f;
static const float VERTS_PER_POLY = 6.f;
static const float DETAIL_SAMPLE_DIST = 6.f;
static const float DETAIL_SAMPLE_MAX_ERROR = 1.f;
static const int TILE_SIZE = 128;

std::mutex global_mutex;

static void add_tile_mesh_rows(const int y, const int tw, const float tcs, const float *bmin, const float *bmax, const float *verts, const int nverts, const rcChunkyTriMesh *chunky_mesh, const rcConfig *config, dtNavMesh *navmesh);
static uint8_t* build_tile_mesh(const int tx, const int ty, float *bmin, float *bmax, int &data_size, const float *verts, const int nverts, const rcChunkyTriMesh *chunky_mesh, const rcConfig *config);

static inline uint32_t nextpow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static inline uint32_t ilog2(uint32_t v)
{
	uint32_t r = (v > 0xffff) << 4; 
	v >>= r;
	uint32_t shift = (v > 0xff) << 3; 
	v >>= shift; 
	r |= shift;
	shift = (v > 0xf) << 2; 
	v >>= shift; 
	r |= shift;
	shift = (v > 0x3) << 1; 
	v >>= shift; 
	r |= shift;
	r |= (v >> 1);

	return r;
}

Navigation::Navigation()
{
	memset(&m_config, 0, sizeof(m_config));
	m_config.cs = CELL_SIZE;
	m_config.ch = CELL_HEIGHT;
	m_config.walkableSlopeAngle = 60.f;
	m_config.walkableHeight = int(ceilf(2.f / m_config.ch));
	m_config.walkableClimb = int(floorf(0.9f / m_config.ch));
	m_config.walkableRadius = int(ceilf(0.6f / m_config.cs));
	m_config.maxEdgeLen = int(12.f / 0.35f);
	m_config.maxSimplificationError = 1.3f;
	m_config.minRegionArea = int(rcSqr(8));  // Note: area = size*size
	m_config.mergeRegionArea = int(rcSqr(20)); // Note: area = size*size
	m_config.maxVertsPerPoly = 6;

	m_config.tileSize = TILE_SIZE;
	m_config.borderSize = m_config.walkableRadius + 3; 

	m_config.width = m_config.tileSize + m_config.borderSize*2;
	m_config.height = m_config.tileSize + m_config.borderSize*2;
	m_config.detailSampleDist = DETAIL_SAMPLE_DIST < 0.9f ? 0 : m_config.cs * DETAIL_SAMPLE_DIST;
	m_config.detailSampleMaxError = CELL_HEIGHT * DETAIL_SAMPLE_MAX_ERROR;
	
	m_navmesh = std::make_unique<dtNavMesh>();
	m_query = std::make_unique<dtNavMeshQuery>();
}

bool Navigation::build(const std::vector<float> &vertices, const std::vector<int> &indices)
{
	m_query = std::make_unique<dtNavMeshQuery>();
	m_chunky_mesh = std::make_unique<rcChunkyTriMesh>();

	rcCreateChunkyTriMesh(vertices.data(), indices.data(), indices.size()/3, 256, m_chunky_mesh.get());

	int gw = 0, gh = 0;
	float bmin[3];
	float bmax[3];
	rcCalcBounds(vertices.data(), vertices.size() / 3, bmin, bmax);
	rcCalcGridSize(bmin, bmax, m_config.cs, &gw, &gh);
	m_bounds_min.x = bmin[0];
	m_bounds_min.y = bmin[1];
	m_bounds_min.z = bmin[2];
	m_bounds_max.x = bmax[0];
	m_bounds_max.y = bmax[1];
	m_bounds_max.z = bmax[2];
	const int ts = TILE_SIZE;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;

	// Max tiles and max polys affect how the tile IDs are caculated.
	// There are 22 bits available for identifying a tile and a polygon.
	int tile_bits = rcMin((int)ilog2(nextpow2(tw*th)), 14);
	if (tile_bits > 14) { tile_bits = 14; }
	int poly_bits = 22 - tile_bits;
	m_max_tiles = 1 << tile_bits;
	m_max_polys = 1 << poly_bits;

	m_navmesh = std::make_unique<dtNavMesh>();

	dtNavMeshParams params;
	rcVcopy(params.orig, bmin);
	params.tileWidth = TILE_SIZE * m_config.cs;
	params.tileHeight = TILE_SIZE * m_config.cs;
	params.maxTiles = m_max_tiles;
	params.maxPolys = m_max_polys;
	
	dtStatus status = m_navmesh->init(&params);
	if (dtStatusFailed(status)) {
		logger::ERROR("Build tiled navigation: could not init navmesh");
		return false;
	}
	
	status = m_query->init(m_navmesh.get(), 2048);
	if (dtStatusFailed(status)) {
		logger::ERROR("Build tiled navigation: could not init Detour navmesh query");
		return false;
	}

	build_all_tiles(vertices, indices);

	m_record.insert(m_navmesh.get());

	return true;
}

void Navigation::build_all_tiles(const std::vector<float> &vertices, const std::vector<int> &indices)
{
	const float *bmin = glm::value_ptr(m_bounds_min);
	const float *bmax = glm::value_ptr(m_bounds_max);
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, m_config.cs, &gw, &gh);
	const int ts = TILE_SIZE;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;
	const float tcs = TILE_SIZE * m_config.cs;
	
	std::vector<std::thread> threads;
	
	// Start the build process.
	for (int y = 0; y < th; ++y) {
		threads.push_back(std::thread(add_tile_mesh_rows, y, tw, tcs, bmin, bmax, vertices.data(), vertices.size(), m_chunky_mesh.get(), &m_config, m_navmesh.get()));
	}

	for (std::thread &th : threads) {
		if (th.joinable()) { th.join(); }
	}
}

void Navigation::find_2D_path(const glm::vec2 &startpos, const glm::vec2 &endpos, std::list<glm::vec2> &pathways) const
{
	const glm::vec3 start = { startpos.x, 0.f, startpos.y };
	const glm::vec3 end = { endpos.x, 0.f, endpos.y };

	dtQueryFilter filter;
	filter.setIncludeFlags(0xFFFF);
	filter.setExcludeFlags(0);
	filter.setAreaCost(POLY_AREA_GROUND, 1.f);

	// find the start polygon
	dtPolyRef start_poly;
	float nearest_start[3];
	dtStatus status = m_query->findNearestPoly(glm::value_ptr(start), BOX_EXTENTS, &filter, &start_poly, nearest_start);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) {
		return; 
	}

	// find the end polygon
	dtPolyRef end_poly;
	float nearest_end[3];
	status = m_query->findNearestPoly(glm::value_ptr(end), BOX_EXTENTS, &filter, &end_poly, nearest_end);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) { 
		return; 
	}

	dtPolyRef poly_path[MAX_PATHPOLY];
	int path_count = 0;
	status = m_query->findPath(start_poly, end_poly, nearest_start, nearest_end, &filter, poly_path, &path_count, MAX_PATHPOLY);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) { 
		return; 
	}
	if (path_count == 0) { 
		return; 
	}

	int vert_count = 0;
	float pathdata[MAX_PATHVERT*3];
	status = m_query->findStraightPath(nearest_start, nearest_end, poly_path, path_count, pathdata, NULL, NULL, &vert_count, MAX_PATHVERT);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) { 
		return; 
	}
	if (vert_count < 1) { 
		return; 
	}

	for (int i = 0; i < vert_count*3; i += 3) {
		glm::vec2 pos = { pathdata[i], pathdata[i+2] };
		pathways.push_back(pos);
	}
}

void Navigation::find_3D_path(const glm::vec3 &startpos, const glm::vec3 &endpos, std::vector<glm::vec3> &pathways) const
{
	dtQueryFilter filter;
	filter.setIncludeFlags(0xFFFF);
	filter.setExcludeFlags(0);
	filter.setAreaCost(POLY_AREA_GROUND, 1.f);

	// find the start polygon
	dtPolyRef start_poly;
	float nearest_start[3];
	dtStatus status = m_query->findNearestPoly(glm::value_ptr(startpos), BOX_EXTENTS, &filter, &start_poly, nearest_start);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) {
		return; 
	}

	// find the end polygon
	dtPolyRef end_poly;
	float nearest_end[3];
	status = m_query->findNearestPoly(glm::value_ptr(endpos), BOX_EXTENTS, &filter, &end_poly, nearest_end);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) { 
		return; 
	}

	dtPolyRef poly_path[MAX_PATHPOLY];
	int path_count = 0;
	status = m_query->findPath(start_poly, end_poly, nearest_start, nearest_end, &filter, poly_path, &path_count, MAX_PATHPOLY);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) { 
		return; 
	}
	if (path_count == 0) { 
		return; 
	}

	int vert_count = 0;
	float pathdata[MAX_PATHVERT*3];
	status = m_query->findStraightPath(nearest_start, nearest_end, poly_path, path_count, pathdata, NULL, NULL, &vert_count, MAX_PATHVERT);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) { 
		return; 
	}
	if (vert_count < 1) { 
		return; 
	}

	for (int i = 0; i < vert_count*3; i += 3) {
		glm::vec3 pos = {
			pathdata[i],
			pathdata[i+1],
			pathdata[i+2]
		};
		pathways.push_back(pos);
	}
}

PolySearchResult Navigation::point_on_navmesh(const glm::vec3 &point) const
{
	PolySearchResult result;
	result.found = false;

	dtQueryFilter filter;
	filter.setIncludeFlags(0xFFFF);
	filter.setExcludeFlags(0);
	filter.setAreaCost(POLY_AREA_GROUND, 1.f);

	// find the start polygon
	dtPolyRef poly;
	float nearest[3];
	dtStatus status = m_query->findNearestPoly(glm::value_ptr(point), BOX_EXTENTS, &filter, &poly, nearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) {
		return result; 
	}

	result.found = true;
	result.position = glm::vec3(nearest[0], nearest[1], nearest[2]);
	result.poly = poly;

	return result;
}

static void add_tile_mesh_rows(const int y, const int tw, const float tcs, const float *bmin, const float *bmax, const float *verts, const int nverts, const rcChunkyTriMesh *chunky_mesh, const rcConfig *config, dtNavMesh *navmesh)
{
	for (int x = 0; x < tw; ++x) {
		float tile_min[3];
		tile_min[0] = bmin[0] + x*tcs;
		tile_min[1] = bmin[1];
		tile_min[2] = bmin[2] + y*tcs;
		
		float tile_max[3];
		tile_max[0] = bmin[0] + (x+1)*tcs;
		tile_max[1] = bmax[1];
		tile_max[2] = bmin[2] + (y+1)*tcs;
		
		int data_size = 0;
		Navbuilder builder = Navbuilder(true);
		uint8_t *data = builder.alloc_navdata(x, y, tile_min, tile_max, data_size, verts, nverts, chunky_mesh, config);
		std::lock_guard<std::mutex> guard(global_mutex);
		if (data) {
			// Remove any previous data (navmesh owns and deletes the data).
			navmesh->removeTile(navmesh->getTileRefAt(x, y, 0), 0, 0);
			// Let the navmesh own the data.
			dtStatus status = navmesh->addTile(data, data_size, DT_TILE_FREE_DATA, 0, 0);
			if (dtStatusFailed(status)) { dtFree(data); }
		}
	}
}

uint8_t* Navbuilder::alloc_navdata(const int tx, const int ty, float *bmin, float *bmax, int &data_size, const float *verts, const int nverts, const rcChunkyTriMesh *chunky_mesh, const rcConfig *config)
{
	// Expand the heighfield bounding box by border size to find the extents of geometry we need to build this tile.
	//
	// This is done in order to make sure that the navmesh tiles connect correctly at the borders,
	// and the obstacles close to the border work correctly with the dilation process.
	// No polygons (or contours) will be created on the border area.
	//
	// IMPORTANT!
	//
	//   :''''''''':
	//   : +-----+ :
	//   : |     | :
	//   : |     |<--- tile to build
	//   : |     | :  
	//   : +-----+ :<-- geometry needed
	//   :.........:
	//
	// You should use this bounding box to query your input geometry.
	//
	// For example if you build a navmesh for terrain, and want the navmesh tiles to match the terrain tile size
	// you will need to pass in data from neighbour terrain tiles too! In a simple case, just pass in all the 8 neighbours,
	// or use the bounding box below to only pass in a sliver of each of the 8 neighbours.
	bmin[0] -= config->borderSize*config->cs;
	bmin[2] -= config->borderSize*config->cs;
	bmax[0] += config->borderSize*config->cs;
	bmax[2] += config->borderSize*config->cs;
	
	// Allocate voxel heightfield where we rasterize our input data to.
	solid = rcAllocHeightfield();
	if (!solid) {
		logger::ERROR("buildNavigation: Out of memory 'solid'");
		return 0;
	}
	if (!rcCreateHeightfield(context, *solid, config->width, config->height, bmin, bmax, config->cs, config->ch)) {
		logger::ERROR("buildNavigation: Could not create solid heightfield");
		return 0;
	}
	
	// Allocate array that can hold triangle flags.
	// If you have multiple meshes you need to process, allocate
	// and array which can hold the max number of triangles you need to process.
	triareas = new uint8_t[chunky_mesh->maxTrisPerChunk];
	if (!triareas) {
		logger::ERROR("buildNavigation: Out of memory 'triareas'");
		return 0;
	}
	
	float tbmin[2], tbmax[2];
	tbmin[0] = bmin[0];
	tbmin[1] = bmin[2];
	tbmax[0] = bmax[0];
	tbmax[1] = bmax[2];
	int cid[512];
	const int ncid = rcGetChunksOverlappingRect(chunky_mesh, tbmin, tbmax, cid, 512);
	if (!ncid) { 
		return 0; 
	}
	
	int tile_tri_count = 0;
	for (int i = 0; i < ncid; ++i) {
		const rcChunkyTriMeshNode &node = chunky_mesh->nodes[cid[i]];
		const int *ctris = &chunky_mesh->tris[node.i*3];
		const int nctris = node.n;
		
		tile_tri_count += nctris;
		
		memset(triareas, 1, nctris*sizeof(uint8_t));
		//rcMarkWalkableTriangles(context, config->walkableSlopeAngle, verts, nverts, ctris, nctris, triareas);
		if (!rcRasterizeTriangles(context, verts, nverts, ctris, triareas, nctris, *solid, config->walkableClimb)) { 
			logger::ERROR("could not rasterize tris");
			return 0; 
		}
	}
	
	// Once all geometry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	//if (m_filterLowHangingObstacles)
	rcFilterLowHangingWalkableObstacles(context, config->walkableClimb, *solid);
	//if (m_filterLedgeSpans)
	rcFilterLedgeSpans(context, config->walkableHeight, config->walkableClimb, *solid);
	//if (m_filterWalkableLowHeightSpans)
	rcFilterWalkableLowHeightSpans(context, config->walkableHeight, *solid);
	
	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	chf = rcAllocCompactHeightfield();
	if (!chf) {
		logger::ERROR("buildNavigation: Out of memory 'chf'");
		return 0;
	}
	if (!rcBuildCompactHeightfield(context, config->walkableHeight, config->walkableClimb, *solid, *chf)) {
		logger::ERROR("buildNavigation: Could not build compact data");
		return 0;
	}
	
	rcFreeHeightField(solid);
	solid = 0;

	// Erode the walkable area by agent radius.
	if (!m_ignore_agent_radius) {
		if (!rcErodeWalkableArea(context, config->walkableRadius, *chf)) {
			logger::ERROR("buildNavigation: Could not erode");
			return 0;
		}
	}

	// (Optional) Mark areas.
	/*
	const ConvexVolume* vols = m_geom->getConvexVolumes();
	for (int i  = 0; i < m_geom->getConvexVolumeCount(); ++i)
		rcMarkConvexPolyArea(context, vols[i].verts, vols[i].nverts, vols[i].hmin, vols[i].hmax, (uint8_t)vols[i].area, *chf);
	
		*/
	
	// Partition the heightfield so that we can use simple algorithm later to triangulate the walkable areas.
	// There are 3 martitioning methods, each with some pros and cons:
	// 1) Watershed partitioning
	//   - the classic Recast partitioning
	//   - creates the nicest tessellation
	//   - usually slowest
	//   - partitions the heightfield into nice regions without holes or overlaps
	//   - the are some corner cases where this method creates produces holes and overlaps
	//      - holes may appear when a small obstacles is close to large open area (triangulation can handle this)
	//      - overlaps may occur if you have narrow spiral corridors (i.e stairs), this make triangulation to fail
	//   * generally the best choice if you precompute the nacmesh, use this if you have large open areas
	// 2) Monotone partioning
	//   - fastest
	//   - partitions the heightfield into regions without holes and overlaps (guaranteed)
	//   - creates long thin polygons, which sometimes causes paths with detours
	//   * use this if you want fast navmesh generation
	// 3) Layer partitoining
	//   - quite fast
	//   - partitions the heighfield into non-overlapping regions
	//   - relies on the triangulation code to cope with holes (thus slower than monotone partitioning)
	//   - produces better triangles than monotone partitioning
	//   - does not have the corner cases of watershed partitioning
	//   - can be slow and create a bit ugly tessellation (still better than monotone)
	//     if you have large open areas with small obstacles (not a problem if you use tiles)
	//   * good choice to use for tiled navmesh with medium and small sized tiles
	
	// Partition the walkable surface into simple regions without holes.
	if (!rcBuildLayerRegions(context, *chf, config->borderSize, config->minRegionArea)) {
		logger::ERROR("buildNavigation: Could not build layer regions");
		return 0;
	}
	 	
	// Create contours.
	cset = rcAllocContourSet();
	if (!cset) {
		logger::ERROR("buildNavigation: Out of memory 'cset'");
		return 0;
	}
	if (!rcBuildContours(context, *chf, config->maxSimplificationError, config->maxEdgeLen, *cset)) {
		logger::ERROR("buildNavigation: Could not create contours");
		return 0;
	}
	
	if (cset->nconts == 0) { return 0; }
	
	// Build polygon navmesh from the contours.
	pmesh = rcAllocPolyMesh();

	if (!rcBuildPolyMesh(context, *cset, config->maxVertsPerPoly, *pmesh)) {
		logger::ERROR("buildNavigation: Could not triangulate contours");
		return 0;
	}
	
	// Build detail mesh.
	dmesh = rcAllocPolyMeshDetail();
	
	if (!rcBuildPolyMeshDetail(context, *pmesh, *chf, config->detailSampleDist, config->detailSampleMaxError, *dmesh)) {
		logger::ERROR("buildNavigation: Could build polymesh detail");
		return 0;
	}
	
	rcFreeCompactHeightfield(chf);
	chf = 0;
	rcFreeContourSet(cset);
	cset = 0;
	
	uint8_t *navdata = 0;
	int navdata_size = 0;
	if (config->maxVertsPerPoly <= DT_VERTS_PER_POLYGON) {
		if (pmesh->nverts >= 0xffff) {
			// The vertex indices are ushorts, and cannot point to more than 0xffff vertices.
			logger::ERROR("Too many vertices per tile");
			return 0;
		}
		
		// Update poly flags from areas.
		for (int i = 0; i < pmesh->npolys; ++i) {
			if (pmesh->areas[i] == RC_WALKABLE_AREA) {
				pmesh->areas[i] = POLY_AREA_GROUND;
			}
			
			if (pmesh->areas[i] == POLY_AREA_GROUND || pmesh->areas[i] == POLY_AREA_GRASS || pmesh->areas[i] == POLY_AREA_ROAD) {
				pmesh->flags[i] = POLY_FLAGS_WALK;
			} else if (pmesh->areas[i] == POLY_AREA_WATER) {
				pmesh->flags[i] = POLY_FLAGS_SWIM;
			} else if (pmesh->areas[i] == POLY_AREA_DOOR) {
				pmesh->flags[i] = POLY_FLAGS_WALK | POLY_FLAGS_DOOR;
			}
		}
		
		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = pmesh->verts;
		params.vertCount = pmesh->nverts;
		params.polys = pmesh->polys;
		params.polyAreas = pmesh->areas;
		params.polyFlags = pmesh->flags;
		params.polyCount = pmesh->npolys;
		params.nvp = pmesh->nvp;
		params.detailMeshes = dmesh->meshes;
		params.detailVerts = dmesh->verts;
		params.detailVertsCount = dmesh->nverts;
		params.detailTris = dmesh->tris;
		params.detailTriCount = dmesh->ntris;
		params.walkableHeight = AGENT_HEIGHT;
		params.walkableRadius = AGENT_RADIUS;
		params.walkableClimb = AGENT_MAX_CLIMB;
		params.tileX = tx;
		params.tileY = ty;
		params.tileLayer = 0;
		rcVcopy(params.bmin, pmesh->bmin);
		rcVcopy(params.bmax, pmesh->bmax);
		params.cs = config->cs;
		params.ch = config->ch;
		params.buildBvTree = true;
		
		if (!dtCreateNavMeshData(&params, &navdata, &navdata_size)) {
			logger::ERROR("Could not build Detour navmesh");
			return 0;
		}		
	}
	
	data_size = navdata_size;

	return navdata;
}

};
