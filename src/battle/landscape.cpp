#include <iostream>
#include <random>
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <list>
#include <map>
#include <functional>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../util/image.h"
#include "cadastre.h"

#include "landscape.h"

namespace carto {

// TODO calculate from AABB
static const float WALL_GATE_DIST = 8.F;
static const float WALL_SEG_DIST = 4.F;
static const float MIN_TOWER_DIFFERENCE = 5.F;

static bool larger_house(const LandscapeObject *a, const LandscapeObject *b);
static geom::Quadrilateral building_box(const glm::vec2 &center, const glm::vec2 &halfwidths, float angle);
static bool quad_quad_intersection(const geom::Quadrilateral &A, const geom::Quadrilateral &B);

void Landscaper::clear()
{
	// clear house transforms
	for (auto &house : houses) {
		house.second.transforms.clear();
	}
	fortification.wall_even.transforms.clear();
	fortification.wall_both.transforms.clear();
	fortification.wall_left.transforms.clear();
	fortification.wall_right.transforms.clear();
	fortification.ramp.transforms.clear();
	fortification.gate.transforms.clear();
	fortification.tower.transforms.clear();
}

void Landscaper::generate(int seed, uint32_t tile, uint8_t town_size, const util::Image<float> &heightmap)
{
	m_cadastre.generate(seed, tile, bounds, town_size);

	bool walled = true;

	// place buildings if town
	if (town_size > 0) {
		spawn_houses(walled, town_size);

		if (walled) {
			spawn_walls(heightmap);
		}
	}
}

void Landscaper::add_house(uint32_t mold_id, const geom::AABB &bounds)
{
	LandscapeObject object;
	object.mold_id = mold_id;

	glm::vec3 size = bounds.max - bounds.min;
	object.bounds = size;

	houses[mold_id] = object;
}
	
void Landscaper::spawn_houses(bool walled, uint8_t town_size)
{
	// create wall cull quads so houses don't go through city walls
	std::vector<geom::Quadrilateral> wall_culls;

	if (walled) {
		for (const auto &d : m_cadastre.districts) {
			for (const auto &sect : d.sections) {
				if (sect->wall) {
					geom::Segment S = { sect->d0->center, sect->d1->center };
					glm::vec2 mid = geom::midpoint(S.A, S.B);
					glm::vec2 direction = glm::normalize(S.B - S.A);
					float angle = atan2(direction.x, direction.y);
					glm::vec2 halfwidths = { 10.f, glm::distance(mid, S.B) };
					geom::Quadrilateral box = building_box(mid, halfwidths, angle);
					wall_culls.push_back(box);
				}
			}
		}
	}

	std::vector<LandscapeObject*> pool;
	for (auto &house : houses) {
		pool.push_back(&houses[house.first]);
	}
	std::sort(pool.begin(), pool.end(), larger_house);

	// no valid houses found early exit
	if (pool.size() < 1) { return; }

	// factor over of house overlap
	// set this to 1 if you don't want houses to overlap
	const float overlap = 1.4f;

	// place the town houses
	for (const auto &district : m_cadastre.districts) {
		for (const auto &parcel : district.parcels) {
			float front = overlap * glm::distance(parcel.quad.b, parcel.quad.a);
			float back = overlap * glm::distance(parcel.quad.c, parcel.quad.d);
			float left = overlap * glm::distance(parcel.quad.b, parcel.quad.c);
			float right = overlap * glm::distance(parcel.quad.a, parcel.quad.d);

			float angle = atan2(parcel.direction.x, parcel.direction.y);

			bool found = false;

			for (auto &house : pool) {
				if ((front > house->bounds.x && back > house->bounds.x) && (left > house->bounds.z && right > house->bounds.z)) {
					// find out if wall overlaps with house
					glm::vec2 halfwidths = {  0.5f * house->bounds.x, 0.5f * house->bounds.z };
					geom::Quadrilateral box = building_box(parcel.centroid, halfwidths, angle);
					bool wall_overlap = false;
					for (const auto &wallbox : wall_culls) {
						if (quad_quad_intersection(box, wallbox)) {
							wall_overlap = true;
							break;
						}
					}
					if (!wall_overlap) {
						LandscapeObjectTransform transform = {
							parcel.centroid,
							angle
						};
						house->transforms.push_back(transform);

						found = true;
					}

					break;
				}
			}

			// place smallest possible house
			if (!found) {
				auto &house = pool.back();
				glm::vec2 halfwidths = {  0.5f * house->bounds.x, 0.5f * house->bounds.z };
				geom::Quadrilateral box = building_box(parcel.centroid, halfwidths, angle);
				bool wall_overlap = false;
				for (const auto &wallbox : wall_culls) {
					if (quad_quad_intersection(box, wallbox)) {
						wall_overlap = true;
						break;
					}
				}
				if (!wall_overlap) {
					LandscapeObjectTransform transform = {
						parcel.centroid,
						angle
					};
					house->transforms.push_back(transform);
				}
			}
		}
	}
}
	
void Landscaper::spawn_walls(const util::Image<float> &heightmap)
{
	// place the wall gates
	for (const auto &wall : m_cadastre.walls) {
		if (wall.gate) {
			glm::vec2 mid = geom::midpoint(wall.S.A, wall.S.B);
			glm::vec2 direction = glm::normalize(wall.S.B - wall.S.A);
			float angle = atan2(direction.x, direction.y);
			LandscapeObjectTransform transform = { mid, angle };
			fortification.gate.transforms.push_back(transform);
		}
	}

	// place the wall towers
	for (const auto &wall : m_cadastre.walls) {
		glm::vec2 direction = glm::normalize(wall.S.B - wall.S.A);
		float angle = atan2(direction.x, direction.y);
		glm::vec2 position = { wall.S.A.x, wall.S.A.y };
		LandscapeObjectTransform transform = { position, angle };
		fortification.tower.transforms.push_back(transform);
	}

	// place the wall segments
	for (const auto &wall : m_cadastre.walls) {
		if (wall.gate) {
			glm::vec2 direction = glm::normalize(wall.S.B - wall.S.A);
			glm::vec2 mid = geom::midpoint(wall.S.A, wall.S.B);
			geom::Segment left = { wall.S.A, mid - (WALL_GATE_DIST * direction) };
			fill_wall(left, heightmap, fortification);
			geom::Segment right = { mid + (WALL_GATE_DIST * direction), wall.S.B };
			fill_wall(right, heightmap, fortification);
		} else {
			fill_wall(wall.S, heightmap, fortification);
		}
	}
}
	
void Landscaper::fill_wall(const geom::Segment &segment, const util::Image<float> &heightmap, LandscapeFortification &output)
{
	glm::vec2 direction = glm::normalize(segment.B - segment.A);
	float angle = atan2(direction.x, direction.y);

	glm::vec2 scale = bounds.max - bounds.min;

	glm::vec2 origin = segment.A;
	float d = (1.f / WALL_SEG_DIST) * glm::distance(segment.A, segment.B);
	// find height of the end positions of the segment on the heightmap
	float h0 = heightmap.sample_relative(segment.A.x / scale.x, segment.A.y / scale.y, util::CHANNEL_RED);
	float h1 = heightmap.sample_relative(segment.B.x / scale.x, segment.B.y / scale.y, util::CHANNEL_RED);

	glm::vec2 position = origin;
	// if the end positions differ too much in height use wall segments with stairs
	if (fabs(h0 - h1) < MIN_TOWER_DIFFERENCE) {
		for (int i = 0; i < int(d); i++) {
			LandscapeObjectTransform transform = { origin, angle };
			// if we are in the middle of the wall segment place a ramp
			// else place a normal wall
			if (i == int(d) / 2) {
				output.ramp.transforms.push_back(transform);
			} else {
				output.wall_both.transforms.push_back(transform);
			}
			origin += WALL_SEG_DIST * direction;
		}
	} else { 
		// end positions are roughly same in height, use even wall segments
		for (int i = 0; i < int(d); i++) {
			LandscapeObjectTransform transform = { origin, angle };
			glm::vec2 offset = WALL_SEG_DIST * direction;
			if (i == int(d) / 2) {
				output.ramp.transforms.push_back(transform);
			} else {
				// check if the wall segments next to this one are on a different height 
				// if they are not place an even wall segment
				// otherwise check the height difference and pick a wall based on that
				glm::vec2 left_pos = origin - offset;
				glm::vec2 right_pos = origin + offset;
				float left_height = heightmap.sample_relative(left_pos.x / scale.x, left_pos.y / scale.y, util::CHANNEL_RED);
				float right_height = heightmap.sample_relative(right_pos.x / scale.x, right_pos.y / scale.y, util::CHANNEL_RED);
				float height = heightmap.sample_relative(origin.x / scale.x, origin.y / scale.y, util::CHANNEL_RED);
				if (left_height < height && right_height < height) {
					output.wall_both.transforms.push_back(transform);
				} else if (left_height < height) {
					output.wall_left.transforms.push_back(transform);
				} else if (right_height < height) {
					output.wall_right.transforms.push_back(transform);
				} else {
					output.wall_even.transforms.push_back(transform);
				}
			}
			origin += offset;
		}
	}
}

static bool larger_house(const LandscapeObject *a, const LandscapeObject *b)
{
	return (a->bounds.x * a->bounds.z) > (b->bounds.x * b->bounds.z);
}


static geom::Quadrilateral building_box(const glm::vec2 &center, const glm::vec2 &halfwidths, float angle)
{
	glm::vec2 a = { -halfwidths.x, halfwidths.y };
	glm::vec2 b = { -halfwidths.x, -halfwidths.y };
	glm::vec2 c = { halfwidths.x, -halfwidths.y };
	glm::vec2 d = { halfwidths.x, halfwidths.y };

	// apply rotation
	glm::mat2 R = {
		cos(angle), -sin(angle),
		sin(angle), cos(angle)
	};

	geom::Quadrilateral quad = {
		center + R * a,
		center + R * b,
		center + R * c,
		center + R * d
	};

	return quad;
}

static bool projected_axis_test(const glm::vec2 &b1, const glm::vec2 &b2, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3, const glm::vec2 &p4)
{
	float x1, x2, x3, x4;
	float y1, y2, y3, y4;
	if (b1.x == b2.x) {
		x1 = x2 = x3 = x4 = b1.x;
		y1 = p1.y;
		y2 = p2.y;
		y3 = p3.y;
		y4 = p4.y;

		if (b1.y > b2.y) {
			if ((y1 > b1.y && y2 > b1.y && y3 > b1.y && y4 > b1.y) || (y1 < b2.y && y2 < b2.y && y3 < b2.y && y4 < b2.y)) {
				return false;
			}
		} else {
			if ((y1 > b2.y && y2 > b2.y && y3 > b2.y && y4 > b2.y) || (y1 < b1.y && y2 < b1.y && y3 < b1.y && y4 < b1.y)) {
				return false;
			}
		}
		return true;
	} else if (b1.y == b2.y) {
		x1 = p1.x;
		x2 = p2.x;
		x3 = p3.x;
		x4 = p4.x;
		y1 = y2 = y3 = y4 = b1.y;
	} else {
		float a = (b1.y - b2.y) / (b1.x - b2.x);
		float ia = 1 / a;
		float t1 = b2.x * a - b2.y;
		float t2 = 1 / (a + ia);

		x1 = (p1.y + t1 + p1.x * ia) * t2;
		x2 = (p2.y + t1 + p2.x * ia) * t2;
		x3 = (p3.y + t1 + p3.x * ia) * t2;
		x4 = (p4.y + t1 + p4.x * ia) * t2;

		y1 = p1.y + (p1.x - x1) * ia;
		y2 = p2.y + (p2.x - x2) * ia;
		y3 = p3.y + (p3.x - x3) * ia;
		y4 = p4.y + (p4.x - x4) * ia;
	}

	if (b1.x > b2.x) {
		if ((x1 > b1.x && x2 > b1.x && x3 > b1.x && x4 > b1.x) || (x1 < b2.x && x2 < b2.x && x3 < b2.x && x4 < b2.x)) {
			return false;
		}
	} else {
		if ((x1 > b2.x && x2 > b2.x && x3 > b2.x && x4 > b2.x) || (x1 < b1.x && x2 < b1.x && x3 < b1.x && x4 < b1.x)) {
			return false;
		}
	}

	return true;
}

static bool quad_quad_intersection(const geom::Quadrilateral &A, const geom::Quadrilateral &B)
{
	if (!projected_axis_test(A.a, A.b, B.a, B.b, B.c, B.d)) {
		return false;
	}

	if (!projected_axis_test(A.b, A.c, B.a, B.b, B.c, B.d)) {
		return false;
	}

	if (!projected_axis_test(B.a, B.b, A.a, A.b, A.c, A.d)) {
		return false;
	}

	if (!projected_axis_test(B.b, B.c, A.a, A.b, A.c, A.d)) {
		return false;
	}

	return true;
}

};
