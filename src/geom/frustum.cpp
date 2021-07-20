#include <iostream>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "frustum.h"

namespace geom {

void Frustum::update(const glm::mat4 &matrix)
{
	glm::mat4 transposed = glm::transpose(matrix);

	planes[LEFT] = glm::vec4(transposed[3] + transposed[0]);
	planes[RIGHT] = glm::vec4(transposed[3] - transposed[0]);
	planes[BOTTOM] = glm::vec4(transposed[3] + transposed[1]);
	planes[TOP] = glm::vec4(transposed[3] - transposed[1]);
	planes[FRONT] = glm::vec4(transposed[3] + transposed[2]);
	planes[BACK] = glm::vec4(transposed[3] - transposed[2]);

	for (auto &plane : planes) {
		float length = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
		plane /= length;
	}
}
	
bool Frustum::sphere_intersects(const glm::vec3 &position, float radius) const
{
	for (const auto &plane : planes) {
		if (glm::dot(glm::vec4(position, radius), plane) + radius < 0.0) {
			return false;
		}
	}

	return true;
}

bool Frustum::sphere_intersects(const glm::vec4 &sphere) const
{
	for (const auto &plane : planes) {
		if (glm::dot(sphere, plane) + sphere.w < 0.0) {
			return false;
		}
	}

	return true;
}

};
