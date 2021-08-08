#pragma once
#include "../geometry/frustum.h"
#include "../util/serialize.h"

namespace util {

class Camera {
public:
	glm::vec3 position = {};
	glm::vec3 direction = {};
	glm::mat4 projection, viewing;
	glm::mat4 VP; // view * project
	float pitch = 0.f; // in radians
	float yaw = 0.f; // in radians
	geom::Frustum frustum;
	glm::vec2 resolution;
public:
	void update_viewing();
	// create perspective projection matrix
	void set_projection(float FOV, float width, float height, float near, float far);
	// change the camera direction vector
	void direct(const glm::vec3 &dir);
	void target(const glm::vec3 &location);
	// add offset to yaw and pitch
	void add_offset(const glm::vec2 &offset);
	// move the camera
	void move_forward(float modifier);
	void move_backward(float modifier);
	void move_left(float modifier);
	void move_right(float modifier);
	void translate(const glm::vec3 &translation);
	void teleport(const glm::vec3 &location);
public:
	glm::vec3 ndc_to_ray(const glm::vec2 &ndc) const;
private:
	void angles_to_direction();
};

// camera saving
template <class Archive>
void save(Archive &archive, const Camera &camera)
{
	archive(camera.position, camera.direction);
}

// camera loading
template <class Archive>
void load(Archive &archive, Camera &camera)
{
	archive(camera.position, camera.direction);

	camera.direct(camera.direction);
}

};
