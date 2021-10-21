#include <iostream>
#include <unordered_map>
#include <memory>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../graphics/mesh.h"
#include "../graphics/texture.h"
#include "../graphics/model.h"
#include "../physics/physical.h"

#include "mold.h"
	
HouseMold::HouseMold(uint32_t id, const gfx::Model *model)
	: id(id), model(model)
{
	// make collision shape
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
	uint16_t offset = 0;
	for (const auto &mesh : model->collision_meshes()) {
		for (const auto &pos : mesh->positions) {
			positions.push_back(pos);
		}
		for (const auto &index : mesh->indices) {
			indices.push_back(index + offset);
		}
		offset = positions.size();
	}

	collision = std::make_unique<fysx::CollisionMesh>(positions, indices);
}
