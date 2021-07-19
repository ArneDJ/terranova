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

void Camera::set_projection(float FOV, float ratio, float near, float far)
{
	projection = glm::perspective(glm::radians(FOV), ratio, near, far);
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
}

void Camera::target(const glm::vec3 &location)
{
	glm::vec3 dir = location - position;
	direct(dir);
}

};
