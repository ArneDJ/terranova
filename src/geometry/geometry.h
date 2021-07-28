#pragma once 
#include <algorithm>

namespace geom {

template <class T> struct Bounding {
	T min = {};
	T max = {};
};

using Rectangle = Bounding<glm::vec2>;

using AABB = Bounding<glm::vec3>;

struct Sphere {
	glm::vec3 center = {};
	float radius = 1.f;
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

inline float max_component(const glm::vec3 &v)
{
	return max(v.x, v.y, v.z);
}

inline float min_component(const glm::vec3 &v)
{
	return min(v.x, v.y, v.z);
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
		0.5f * glm::distance(aabb.min, aabb.max)
	};

	return sphere;
}

inline AABB confined_bounds(const AABB &a, const AABB &b)
{
	AABB bounds = {
		(glm::min)(a.min, b.min),
		(glm::max)(a.max, b.max)
	};

	return bounds;
}

inline bool clockwise(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c)
{
	float wise = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);

	return (wise < 0.f);
}

};
