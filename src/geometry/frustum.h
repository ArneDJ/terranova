#pragma once
#include <array>

namespace geom {

class Frustum {
public:
	enum Side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
	std::array<glm::vec4, 6> planes;
public:
	void update(const glm::mat4 &matrix);
	bool sphere_intersects(const glm::vec3 &position, float radius) const;
};

};
