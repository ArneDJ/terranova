#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace geom {

struct Transform {
	glm::vec3 position = {};
	glm::quat rotation = {};
	glm::vec3 scale = { 1.F, 1.F, 1.F };

	inline glm::mat4 to_matrix() const
	{
		glm::mat4 T = glm::translate(glm::mat4(1.f), position);
		glm::mat4 R = glm::mat4(rotation);
		glm::mat4 S = glm::scale(glm::mat4(1.f), scale);

		return T * R * S;
	}
};

inline Transform bounds_to_transform(const glm::vec3 &min, const glm::vec3 &max)
{
	Transform transform;

	// position is the center of bounding box
	transform.position = 0.5f * (max + min);
	// scale
	transform.scale.x = 0.5f * glm::distance(min.x, max.x);
	transform.scale.y = 0.5f * glm::distance(min.y, max.y);
	transform.scale.z = 0.5f * glm::distance(min.z, max.z);

	return transform;
}

};
