#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace geom {

struct Transform {
	glm::vec3 position = {};
	glm::quat rotation = {};
	glm::vec3 scale = {};

	inline glm::mat4 to_matrix() const
	{
		glm::mat4 T = glm::translate(glm::mat4(1.f), position);
		glm::mat4 R = glm::mat4(rotation);
		glm::mat4 S = glm::scale(glm::mat4(1.f), scale);

		return T * R * S;
	}
};

};
