#pragma once 
#include <algorithm>

namespace geom {

struct Sphere {
	glm::vec3 center = {};
	float radius = 1.f;
};

struct AABB {
	glm::vec3 min = {};
	glm::vec3 max = {};
};

template<typename T>
inline T max(const T &a, const T &b, const T &c)
{
	return std::max(a, std::max(b, c));
}

template<typename T>
inline T min(const T &a, const T &b, const T &c)
{
	return std::min(a, std::min(b, c));
}

inline float midpoint(float a, float b)
{
	return 0.5f * (a + b);
}

inline glm::vec2 midpoint(const glm::vec2 &a, const glm::vec2 &b)
{
	return glm::vec2(0.5f * (a.x + b.x), 0.5f * (a.y + b.y));
}

inline glm::vec3 midpoint(const glm::vec3 &a, const glm::vec3 &b)
{
	return glm::vec3(0.5f * (a.x + b.x), 0.5f * (a.y + b.y), 0.5 * (a.z + b.z));
}

inline Sphere AABB_to_sphere(const AABB &aabb)
{
	Sphere sphere = {
		midpoint(aabb.min, aabb.max),
		glm::distance(aabb.min, aabb.max)
	};

	return sphere;
}

};
