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

struct Segment {
	glm::vec2 A = {};
	glm::vec2 B = {};
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

inline bool point_in_circle(const glm::vec2 &point, const glm::vec2 &origin, float radius)
{
	return ((point.x - origin.x) * (point.x - origin.x)) + ((point.y - origin.y) * (point.y - origin.y)) <= (radius * radius);
}

inline bool point_in_rectangle(const glm::vec2 &p, const Rectangle &r)
{
	return (p.x >= r.min.x && p.x < r.max.x && p.y >= r.min.y && p.y < r.max.y);
}

// http://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/
inline int orient(float x0, float y0, float x1, float y1, float x2, float y2)
{
	return (int(x1) - int(x0))*(int(y2) - int(y0)) - (int(y1) - int(y0))*(int(x2) - int(x0));
}

// TODO check triangle winding
inline bool triangle_overlaps_point(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &pt)
{
	// barycentric coordinates
	int w0 = orient(b.x, b.y, c.x, c.y, pt.x, pt.y);
	int w1 = orient(c.x, c.y, a.x, a.y, pt.x, pt.y);
	int w2 = orient(a.x, a.y, b.x, b.y, pt.x, pt.y);

	return (w0 >= 0 && w1 >= 0 && w2 >= 0);
}

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
inline float sign(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c)
{
	return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}

inline bool segment_intersects_segment(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec2 &d)
{
	// Sign of areas correspond to which side of ab points c and d are
	float a1 = sign(a, b, d); // Compute winding of abd (+ or -)
	float a2 = sign(a, b, c); // To intersect, must have sign opposite of a1

	// If c and d are on different sides of ab, areas have different signs
	if ((a1 * a2) < 0.f) {
		// Compute signs for a and b with respect to segment cd
		float a3 = sign(c, d, a);
		// Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		float a4 = a3 + a2 - a1;
		// Points a and b on different sides of cd if areas have different signs
		if (a3 * a4 < 0.0f) { return true; }
	}

	// Segments not intersecting (or collinear)
	return false;
}

};
