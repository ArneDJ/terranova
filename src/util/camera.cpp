#include <iostream>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"

namespace util {

static const glm::vec3 UP_VECTOR = { 0.F, 1.F, 0.F };
static const float MAX_CAMERA_ANGLE = 1.55F;
static const float MIN_CAMERA_ANGLE = -1.55F;

void Camera::set_projection(float FOV, float width, float height, float near, float far)
{
	resolution = glm::vec2(width, height);
	projection = glm::perspective(glm::radians(FOV), width / height, near, far);
}

void Camera::direct(const glm::vec3 &dir)
{
	direction = glm::normalize(dir);
	// adjust pitch and yaw to direction vector
	pitch = asin(direction.y);
	yaw = atan2(direction.z, direction.x);
}
	
void Camera::add_offset(const glm::vec2 &offset)
{
	yaw += offset.x;
	pitch -= offset.y;

	pitch = glm::clamp(pitch,  MIN_CAMERA_ANGLE, MAX_CAMERA_ANGLE);

	angles_to_direction();
}

void Camera::angles_to_direction()
{
	direction.x = cos(yaw) * cos(pitch);
	direction.y = sin(pitch);
	direction.z = sin(yaw) * cos(pitch);

	direction = glm::normalize(direction);
}

void Camera::move_forward(float modifier)
{
	position += modifier * direction;
}

void Camera::move_backward(float modifier)
{
	position -= modifier * direction;
}

void Camera::move_left(float modifier)
{
	glm::vec3 left = glm::cross(direction, UP_VECTOR);
	position -= modifier * glm::normalize(left);
}

void Camera::move_right(float modifier)
{
	glm::vec3 right = glm::cross(direction, UP_VECTOR);
	position += modifier * glm::normalize(right);
}

void Camera::translate(const glm::vec3 &translation)
{
	position += translation;
}

void Camera::teleport(const glm::vec3 &location)
{
	position = location;
}
	
void Camera::update_viewing()
{
	viewing = glm::lookAt(position, position + direction, UP_VECTOR);
	VP = projection * viewing;

	frustum.update(VP);
}

void Camera::target(const glm::vec3 &location)
{
	glm::vec3 dir = location - position;
	direct(dir);
}

glm::vec3 Camera::ndc_to_ray(const glm::vec2 &ndc) const
{
	glm::vec4 clip = glm::vec4((2.f * ndc.x) / resolution.x - 1.f, 1.f - (2.f * ndc.y) / resolution.y, -1.f, 1.f);

	glm::vec4 worldspace = glm::inverse(projection) * clip;
	glm::vec4 eye = glm::vec4(worldspace.x, worldspace.y, -1.f, 0.f);
	glm::vec3 ray = glm::vec3(glm::inverse(viewing) * eye);

	return glm::normalize(ray);
}
	
};
