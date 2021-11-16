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
#include "cadastre.h"

#include "landscape.h"

namespace carto {

static bool larger_house(const LandscapeObject *a, const LandscapeObject *b);

void Landscaper::clear()
{
	// clear house transforms
	for (auto &house : houses) {
		house.second.transforms.clear();
	}
}

void Landscaper::generate(int seed, uint32_t tile, uint8_t town_size)
{
	m_cadastre.generate(seed, tile, bounds, town_size);

	// place buildings if town
	if (town_size > 0) {
		spawn_houses(false, town_size);
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
	std::vector<LandscapeObject*> pool;
	for (auto &house : houses) {
		pool.push_back(&houses[house.first]);
	}
	std::sort(pool.begin(), pool.end(), larger_house);

	// no valid houses found early exit
	if (pool.size() < 1) { return; }

	// factor over of house overlap
	// set this to 1 if you don't want houses to overlap
	const float overlap = 1.2f;

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
					LandscapeObjectTransform transform = {
						parcel.centroid,
						angle
					};
					house->transforms.push_back(transform);

					found = true;

					break;
				}
			}

			// place smallest possible house
			if (!found) {
				LandscapeObjectTransform transform = {
					parcel.centroid,
					angle
				};
				pool.back()->transforms.push_back(transform);
			}
		}
	}
}

static bool larger_house(const LandscapeObject *a, const LandscapeObject *b)
{
	return (a->bounds.x * a->bounds.z) > (b->bounds.x * b->bounds.z);
}

};
