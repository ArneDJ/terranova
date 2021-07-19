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

	frustum.update(VP);
}

void Camera::target(const glm::vec3 &location)
{
	glm::vec3 dir = location - position;
	direct(dir);
}
	
void Frustum::update(const glm::mat4 &matrix)
{
	glm::mat4 transposed = glm::transpose(matrix);

	planes[LEFT] = glm::vec4(transposed[3] + transposed[0]); // left
	planes[RIGHT] = glm::vec4(transposed[3] - transposed[0]); // right
	planes[BOTTOM] = glm::vec4(transposed[3] + transposed[1]); // bottom
	planes[TOP] = glm::vec4(transposed[3] - transposed[1]); // top
	planes[FRONT] = glm::vec4(transposed[3] + transposed[2]); // near
	planes[BACK] = glm::vec4(transposed[3] - transposed[2]); // far
	/*
	planes[LEFT].x = matrix[0].w + matrix[0].x;
	planes[LEFT].y = matrix[1].w + matrix[1].x;
	planes[LEFT].z = matrix[2].w + matrix[2].x;
	planes[LEFT].w = matrix[3].w + matrix[3].x;

	planes[RIGHT].x = matrix[0].w - matrix[0].x;
	planes[RIGHT].y = matrix[1].w - matrix[1].x;
	planes[RIGHT].z = matrix[2].w - matrix[2].x;
	planes[RIGHT].w = matrix[3].w - matrix[3].x;

	planes[TOP].x = matrix[0].w - matrix[0].y;
	planes[TOP].y = matrix[1].w - matrix[1].y;
	planes[TOP].z = matrix[2].w - matrix[2].y;
	planes[TOP].w = matrix[3].w - matrix[3].y;

	planes[BOTTOM].x = matrix[0].w + matrix[0].y;
	planes[BOTTOM].y = matrix[1].w + matrix[1].y;
	planes[BOTTOM].z = matrix[2].w + matrix[2].y;
	planes[BOTTOM].w = matrix[3].w + matrix[3].y;

	planes[BACK].x = matrix[0].w + matrix[0].z;
	planes[BACK].y = matrix[1].w + matrix[1].z;
	planes[BACK].z = matrix[2].w + matrix[2].z;
	planes[BACK].w = matrix[3].w + matrix[3].z;

	planes[FRONT].x = matrix[0].w - matrix[0].z;
	planes[FRONT].y = matrix[1].w - matrix[1].z;
	planes[FRONT].z = matrix[2].w - matrix[2].z;
	planes[FRONT].w = matrix[3].w - matrix[3].z;
	*/

	for (auto &plane : planes) {
		float length = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
		plane /= length;
	}
}
	
bool Frustum::sphere_intersects(const glm::vec3 &position, float radius)
{
	for (const auto &plane : planes) {
		if ((plane.x * position.x) + (plane.y * position.y) + (plane.z * position.z) + plane.w <= -radius) {
			return false;
		}
	}

	return true;
}

};
